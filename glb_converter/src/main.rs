use std::env;
use std::vec;

use anyhow::Ok;
use gltf::Gltf;
use gltf::image::Source;
use serde::Deserialize;
use serde::Serialize;

use bytemuck::Pod;
use bytemuck::Zeroable;

#[derive(Serialize, Deserialize)]
struct Scene {
    pub name: String,
    pub entities_file: String,
    pub environment: Environment,
}

#[derive(Serialize, Deserialize)]
struct Environment {
    pub skybox: Option<uuid::Uuid>,
    pub ambient_color: [f32; 3],
}

#[derive(Serialize, Deserialize)]
pub struct Entity {
    pub id: uuid::Uuid,
    pub name: String,

    pub transform: Option<Transform>,
    pub mesh_renderers: Vec<MeshRenderer>, //Can contain multiple primitives

    pub components: Vec<Components>,
}

#[derive(Serialize, Deserialize, Default)]
pub struct Components {
    pub type_name: String,
    pub id: uuid::Uuid,
    pub data: serde_json::Value,
}

#[derive(Serialize, Deserialize)]
pub struct Transform {
    pub translation: [f32; 3],
    pub rotation: [f32; 4],
    pub scale: [f32; 3],
}

#[derive(Serialize, Deserialize)]
pub struct MeshRenderer {
    pub mesh: uuid::Uuid,
    pub material: uuid::Uuid,
}

#[derive(Serialize, Deserialize)]
enum Texture {
    Albedo(Option<uuid::Uuid>),
    MetallicRoughness(Option<uuid::Uuid>),
    Normal(Option<uuid::Uuid>),
    Emissive(Option<uuid::Uuid>),
}

#[derive(Serialize, Deserialize, Default)]
pub struct Material {
    pub id: uuid::Uuid,
    pub name: String,

    pub textures: Vec<Texture>,

    pub albedo_color: [f32; 4],
    pub metallic: f32,
    pub roughness: f32,
    pub emissive_color: [f32; 3],
    pub custom_material_data: String, //Json  Struct from Gltf Extra
}

#[derive(Serialize, Deserialize, Default)]
pub struct Mesh {
    pub id: uuid::Uuid,
    pub name: String,

    vertex_offset: usize,

    index_offset: usize,

    vertex_size: usize,
    index_size: usize,
}

#[derive(Serialize, Deserialize, Default, Zeroable, Clone, Copy, Pod)]
#[repr(C)]
pub struct MeshData {
    pub vertex_position: [f32; 3],
    _pad0: u32,
    pub normal: [f32; 3],
    _pad1: u32,
    pub color: [f32; 3],
    _pad2: u32,
    pub tex_coords_0: [f32; 2],
    pub tex_coords_1: [f32; 2],
}

const _: () = assert!(std::mem::size_of::<MeshData>() % 16 == 0);

#[derive(Serialize, Default)]
struct Image {
    id: uuid::Uuid,
    name: String,
    mime: String,

    extent: [usize; 2],
}

fn main() -> anyhow::Result<()> {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        eprintln!("No file given");
        return Ok(());
    }

    let scene_path: String = args[1].clone(); //0 is Program name

    println!("Path is: {}", scene_path);

    let file = Gltf::open(scene_path).expect("Could not open file");

    let mut mesh_uuids: Vec<uuid::Uuid> = vec![];
    let mut material_uuids: Vec<uuid::Uuid> = vec![];
    let mut image_uuids: Vec<uuid::Uuid> = vec![];
    let mut node_uuids: Vec<uuid::Uuid> = vec![];

    for m in file.meshes() {
        for p in m.primitives() {
            let uuid = uuid::Uuid::new_v4();
            mesh_uuids.push(uuid);

            let reader = p.reader(|_buffer| file.blob.as_ref().map(|b| &b[..]));

            let vertex_positions: Vec<[f32; 3]> = reader
                .read_positions()
                .map(|iter| iter.collect())
                .unwrap_or_default();

            let indices: Vec<u32> = reader
                .read_indices()
                .map(|r| r.into_u32().collect())
                .unwrap_or_default();

            let normals: Vec<[f32; 3]> = reader
                .read_normals()
                .map(|iter| iter.collect())
                .unwrap_or_default();

            let colors: Vec<[f32; 3]> = reader
                .read_colors(0)
                .map(|iter| iter.into_rgb_f32().map(|c| [c[0], c[1], c[2]]).collect())
                .unwrap_or_else(|| vec![[1.0, 1.0, 1.0]; vertex_positions.len()]);

            let tex_coords_0: Vec<[f32; 2]> = reader
                .read_tex_coords(0)
                .map(|iter| iter.into_f32().map(|uv| [uv[0], uv[1]]).collect())
                .unwrap_or_else(|| vec![[0.0, 0.0]; vertex_positions.len()]);

            let tex_coords_1: Vec<[f32; 2]> = reader
                .read_tex_coords(1)
                .map(|iter| iter.into_f32().map(|uv| [uv[0], uv[1]]).collect())
                .unwrap_or_else(|| vec![[0.0, 0.0]; vertex_positions.len()]);

            let mut mesh_data: Vec<MeshData> = Vec::with_capacity(vertex_positions.len());

            for i in 0..vertex_positions.len() {
                let data: MeshData = MeshData {
                    vertex_position: vertex_positions[i],
                    _pad0: 0,
                    normal: normals[i],
                    _pad1: 0,
                    color: colors[i],
                    _pad2: 0,
                    tex_coords_0: tex_coords_0[i],
                    tex_coords_1: tex_coords_1[i],
                };

                mesh_data.push(data);
            }

            //Construct Mesh Struct
            let mut total_offset: usize = 0;

            let mut mesh: Mesh = Mesh::default();
            mesh.name = m.name().unwrap_or_default().to_string();
            mesh.id = uuid;

            mesh.vertex_size = mesh_data.len() * std::mem::size_of::<MeshData>();
            mesh.vertex_offset = total_offset;

            total_offset += mesh.vertex_size;

            mesh.index_offset = total_offset;
            mesh.index_size = indices.len() * std::mem::size_of::<u32>();

            let mesh_file = format!("meshes/{}.mesh.json", uuid);
            std::fs::create_dir_all(std::path::Path::new(&mesh_file).parent().unwrap())?;
            std::fs::write(&mesh_file, serde_json::to_string_pretty(&mesh)?)?;

            let mut byte_mesh_data: Vec<u8> = vec![];

            byte_mesh_data.extend_from_slice(bytemuck::cast_slice(&mesh_data));
            byte_mesh_data.extend_from_slice(bytemuck::cast_slice(&indices));

            let mesh_data_file = format!("meshes/{}.mesh_data.bin", uuid);
            std::fs::create_dir_all(std::path::Path::new(&mesh_data_file).parent().unwrap())?;
            std::fs::write(&mesh_data_file, &byte_mesh_data)?;
        }
    }

    for img in file.images() {
        let uuid = uuid::Uuid::new_v4();
        image_uuids.push(uuid);

        let mut image = Image::default();

        image.id = uuid;
        image.name = img.name().unwrap().to_string();

        let mut img_data: Vec<u8>;

        match img.source() {
            Source::View { view, mime_type } => {
                image.mime = mime_type.to_string();

                let start = view.offset();
                let end = start + view.length();

                let blob: &Vec<u8> = file.blob.as_ref().expect("No Glb Blob");
                let buffer: &[u8] = &blob[start..end];

                let img = image::load_from_memory(buffer).expect("Failed to decode image");

                image.extent = [img.width() as usize, img.height() as usize];

                img_data = img.to_rgba8().into_raw();
            }

            _ => {
                println!("Img must be embedded for now");
                return Ok(());
            }
        }

        let img_file = format!("images/{}.img.json", uuid);
        std::fs::create_dir_all(std::path::Path::new(&img_file).parent().unwrap())?;
        std::fs::write(&img_file, serde_json::to_string_pretty(&image)?)?;

        let img_data_file = format!("images/{}.img_data.bin", uuid);
        std::fs::create_dir_all(std::path::Path::new(&img_data_file).parent().unwrap())?;
        std::fs::write(&img_data_file, &img_data)?;
    }

    {
        let mut all_materials: Vec<Material> = Vec::with_capacity(file.materials().len());

        for m in file.materials() {
            let uuid = uuid::Uuid::new_v4();
            material_uuids.push(uuid);

            let mut mat = Material::default();

            let albedo_tex = m.pbr_metallic_roughness().base_color_texture();

            if (albedo_tex).is_some() {
                mat.textures.push(Texture::Albedo(Some(
                    image_uuids[albedo_tex.unwrap().texture().index()],
                )));
            } else {
                mat.textures.push(Texture::Albedo(None));
            }

            let metallic_roughness_tex = m.pbr_metallic_roughness().metallic_roughness_texture();

            if metallic_roughness_tex.is_some() {
                mat.textures.push(Texture::MetallicRoughness(Some(
                    image_uuids[metallic_roughness_tex.unwrap().texture().index()],
                )));
            } else {
                mat.textures.push(Texture::MetallicRoughness(None));
            }

            let emissive_tex = m.emissive_texture();

            if emissive_tex.is_some() {
                mat.textures
                    .push(Texture::Emissive(Some(image_uuids[emissive_tex.unwrap().texture().index()])));
            } else {
                mat.textures.push(Texture::Emissive(None));
            }

            let normal_tex = m.normal_texture();

            if normal_tex.is_some() {
                mat.textures
                    .push(Texture::Normal(Some(image_uuids[normal_tex.unwrap().texture().index()])));
            } else {
                mat.textures.push(Texture::Normal(None));
            }

            mat.albedo_color = m.pbr_metallic_roughness().base_color_factor();
            mat.id = uuid;
            mat.emissive_color = m.emissive_factor();
            mat.metallic = m.pbr_metallic_roughness().metallic_factor();
            mat.roughness = m.pbr_metallic_roughness().roughness_factor();
            mat.name = m.name().unwrap().to_string();

            all_materials.push(mat);
        }

        let material_file = format!("materials/materials.json");
        std::fs::create_dir_all(std::path::Path::new(&material_file).parent().unwrap())?;
        std::fs::write(
            &material_file,
            serde_json::to_string_pretty(&all_materials)?,
        )?;
    }

    for _n in file.nodes() {
        let uuid = uuid::Uuid::new_v4();
        node_uuids.push(uuid);
    }

    {
        let mut all_entities: Vec<Entity> = Vec::with_capacity(file.nodes().len());

        for (i, node) in file.nodes().enumerate() {
            let node_uuid = node_uuids[i];

            let (translation, rotation, scale) = node.transform().decomposed();

            let mesh_renderers: Vec<MeshRenderer> = node
                .mesh()
                .map(|mesh| {
                    mesh.primitives()
                        .enumerate()
                        .map(|(prim_idx, primitive)| {
                            let mesh_uuid = mesh_uuids[mesh.index() + prim_idx];

                            // Material UUID
                            let material_id = primitive
                                .material()
                                .index()
                                .map(|idx| material_uuids[idx])
                                .unwrap_or(uuid::Uuid::new_v4());

                            MeshRenderer {
                                mesh: mesh_uuid,
                                material: material_id,
                            }
                        })
                        .collect::<Vec<MeshRenderer>>()
                })
                .unwrap_or_default();

            let entity = Entity {
                id: node_uuid,
                name: node.name().unwrap_or("Entity").to_string(),
                transform: Some(Transform {
                    translation: translation,
                    rotation: rotation,
                    scale: scale,
                }),
                mesh_renderers,
                components: vec![],
            };

            all_entities.push(entity);
        }

        let entity_file = format!("entities/entities.json");
        std::fs::create_dir_all(std::path::Path::new(&entity_file).parent().unwrap())?;
        std::fs::write(&entity_file, serde_json::to_string_pretty(&all_entities)?)?;
    }

    return Ok(());
}
