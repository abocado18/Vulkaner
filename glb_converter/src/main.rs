use std::env;

use anyhow::Ok;
use gltf::Gltf;
use gltf::image::Source;
use serde::Deserialize;
use serde::Serialize;

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

#[derive(Serialize, Deserialize, Default)]
pub struct Mesh {
    pub id: uuid::Uuid,
    pub name: String,
    pub vertex_positions: Vec<[f32; 3]>,
    pub colors: Vec<[f32; 3]>,
    pub normals: Vec<[f32; 3]>,
    pub tex_coords_0: Option<Vec<[f32; 2]>>,
    pub tex_coords_1: Option<Vec<[f32; 2]>>,
    pub indices: Vec<u32>,
}

#[derive(Serialize, Default)]
struct Image {
    id: uuid::Uuid,
    name: String,
    mime: String,
    data: Vec<u8>,
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
    let mut texture_uuids: Vec<uuid::Uuid> = vec![];
    let mut node_uuids: Vec<uuid::Uuid> = vec![];

    for m in file.meshes() {
        for p in m.primitives() {
            let mut mesh: Mesh = Mesh::default();

            let uuid = uuid::Uuid::new_v4();
            mesh_uuids.push(uuid);

            mesh.id = uuid;

            mesh.name = m.name().unwrap().to_string();

            let reader = p.reader(|_buffer| file.blob.as_ref().map(|b| &b[..]));

            mesh.vertex_positions = reader
                .read_positions()
                .map(|iter| iter.collect())
                .unwrap_or_default();

            mesh.colors = reader
                .read_colors(0)
                .map(|iter| iter.into_rgb_f32().map(|c| [c[0], c[1], c[2]]).collect())
                .unwrap_or_else(|| vec![[1.0, 1.0, 1.0]]);

            mesh.normals = reader
                .read_normals()
                .map(|iter| iter.collect())
                .unwrap_or_default();

            mesh.tex_coords_0 = reader
                .read_tex_coords(0)
                .map(|iter| iter.into_f32().map(|uv| [uv[0], uv[1]]).collect());

            mesh.tex_coords_1 = reader
                .read_tex_coords(1)
                .map(|iter| iter.into_f32().map(|uv| [uv[0], uv[1]]).collect());

            mesh.indices = reader
                .read_indices()
                .map(|r| r.into_u32().collect())
                .unwrap_or_default();

            let mesh_file = format!("meshes/{}.mesh.json", uuid);
            std::fs::create_dir_all(std::path::Path::new(&mesh_file).parent().unwrap())?;
            std::fs::write(&mesh_file, serde_json::to_string_pretty(&mesh)?)?;
        }
    }

    for img in file.images() {
        let uuid = uuid::Uuid::new_v4();
        image_uuids.push(uuid);

        let mut image = Image::default();

        image.id = uuid;
        image.name = img.name().unwrap().to_string();

        match img.source() {
            Source::View { view, mime_type } => {
                image.mime = mime_type.to_string();

                let start = view.offset();
                let end = start + view.length();

                let blob: &Vec<u8> = file.blob.as_ref().expect("No Glb Blob");
                let buffer: &[u8] = &blob[start..end];

                let length = end - start;

                image.data = buffer.to_vec();
            }

            _ => {
                println!("Img must be embedded for now");
                return Ok(());
            }
        }

        let img_file = format!("images/{}.entity.json", uuid);
        std::fs::create_dir_all(std::path::Path::new(&img_file).parent().unwrap())?;
        std::fs::write(&img_file, serde_json::to_string_pretty(&image)?)?;

        
    }

    for t in file.textures() {
        let uuid = uuid::Uuid::new_v4();
        texture_uuids.push(uuid);
    }

    for m in file.materials() {
        let uuid = uuid::Uuid::new_v4();
        material_uuids.push(uuid);
    }

    for n in file.nodes() {
        let uuid = uuid::Uuid::new_v4();
        node_uuids.push(uuid);
    }

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

        let entity_file = format!("entities/{}.entity.json", node_uuid);
        std::fs::create_dir_all(std::path::Path::new(&entity_file).parent().unwrap())?;
        std::fs::write(&entity_file, serde_json::to_string_pretty(&entity)?)?;
    }

    return Ok(());
}
