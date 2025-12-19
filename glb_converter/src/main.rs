use std::collections::HashMap;
use std::env;
use std::sync::atomic::AtomicU32;
use std::sync::atomic::Ordering;
use std::vec;

use anyhow::Ok;
use bytemuck::Pod;
use bytemuck::Zeroable;
use cgmath::InnerSpace;
use cgmath::num_traits::NumAssignOps;
use gltf::image::Source;
use serde::Deserialize;
use serde::Serialize;

#[derive(Serialize, Deserialize)]
struct Entity {
    id: u32,
    children: Vec<u32>,
    parent: i32,
    components: HashMap<String, serde_json::Value>,
}

#[derive(Serialize, Deserialize, Default, Clone)]
struct Material {
    id: u32,
    albedo: [f32; 4],
    roughness: f32,
    metallic: f32,
    ao: f32,
    lighting_model: String, //Gets converted to correct id when loading
}

#[repr(C)]
#[derive(Clone, Copy, Pod, Zeroable)]
struct Vertex {
    position: [f32; 3],
    _pad0: u32,
    color: [f32; 3],
    _pad1: u32,
    normals: [f32; 3],
    _pad2: u32,
    tangent: [f32; 4],
    uv: [f32; 4],
}

impl Entity {
    pub fn default() -> Self {
        static COUNTER: AtomicU32 = AtomicU32::new(0);

        let id = COUNTER.fetch_add(1, Ordering::Relaxed);

        Entity {
            id: id,
            children: vec![],
            components: HashMap::new(),
            parent: -1,
        }
    }
}

fn main() -> anyhow::Result<()> {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        eprintln!("No file given");
        return Ok(());
    }

    let scene_path: String = args[1].clone(); //0 is Program name

    println!("Path is: {}", scene_path);

    let (file, buffers, images) = gltf::import(scene_path).unwrap();

    process_scene(&file);

    process_materials(&file, &buffers);

    process_meshes(&file, &buffers);

    process_images(&file, &images);

    return Ok(());
}

fn process_materials(scene: &gltf::Document, buffers: &Vec<gltf::buffer::Data>) {
    for m in scene.materials() {
        let mut mat: Material = Material::default();

        mat.id = m.index().unwrap() as u32;
        mat.albedo = m.pbr_metallic_roughness().base_color_factor();
        mat.metallic = m.pbr_metallic_roughness().metallic_factor();
        mat.roughness = m.pbr_metallic_roughness().roughness_factor();

        if let Some(extra) = m.extras() {
            mat.lighting_model = extra.to_string();
        } else {
            mat.lighting_model = "Pbr".to_string();
        }

        let _ = std::fs::create_dir_all("scene/materials");
        let file_path = "scene/materials/".to_string() + &mat.id.to_string() + &".json".to_string();
        let _ = std::fs::write(file_path, serde_json::to_string_pretty(&mat).unwrap());
    }
}

fn process_meshes(scene: &gltf::Document, buffers: &Vec<gltf::buffer::Data>) {
    for mesh in scene.meshes() {
        let mut mesh_bin_data: Vec<u8> = Vec::default();

        let mut submesh_index_offset: Vec<u32> = Vec::default();
        submesh_index_offset.resize(mesh.primitives().len(), 0);
        let mut submesh_range: Vec<[u32; 2]> = Vec::default();
        submesh_range.resize(mesh.primitives().len(), [0, 0]);

        let mut number_indices : Vec<u32> = Vec::default();
        number_indices.resize(mesh.primitives().len(), 0);

        let mesh_index = mesh.index();

        let primitive_vec: Vec<gltf::Primitive> = mesh.primitives().collect();

        for i in 0..mesh.primitives().len() {
            let primitive = &primitive_vec[i];

            let reader = primitive.reader(|buffer| Some(&buffers[buffer.index()]));

            let position: Vec<[f32; 3]> = reader
                .read_positions()
                .map(|v| v.collect())
                .unwrap_or_default();
            let normals: Vec<[f32; 3]> = reader
                .read_normals()
                .map(|v| v.collect())
                .unwrap_or_default();

            let color: Vec<[f32; 4]> = reader
                .read_colors(0)
                .map(|v| v.into_rgba_f32().collect())
                .unwrap_or_default();

            let uv0: Vec<[f32; 2]> = reader
                .read_tex_coords(0)
                .map(|v| v.into_f32().collect())
                .unwrap_or_default();
            let uv1: Vec<[f32; 2]> = reader
                .read_tex_coords(1)
                .map(|v| v.into_f32().collect())
                .unwrap_or_default();

            let indices: Vec<u32> = reader
                .read_indices()
                .map(|v| v.into_u32().collect())
                .unwrap_or_default();

            let size = position.len();

            let mut vertices: Vec<Vertex> = vec![];

            for i in 0..size {
                let v: Vertex = Vertex {
                    position: *position.get(i).unwrap_or(&[0.0, 0.0, 0.0]),
                    _pad0: 0,
                    normals: *normals.get(i).unwrap_or(&[0.0, 0.0, 0.0]),
                    _pad1: 0,
                    color: color
                        .get(i)
                        .map(|c| [c[0], c[1], c[2]])
                        .unwrap_or([1.0, 1.0, 1.0]),
                    uv: [
                        uv0.get(i).unwrap_or(&[0.0, 0.0])[0],
                        uv0.get(i).unwrap_or(&[0.0, 0.0])[1],
                        uv1.get(i).unwrap_or(&[0.0, 0.0])[0],
                        uv1.get(i).unwrap_or(&[0.0, 0.0])[1],
                    ],
                    _pad2: 0,
                    tangent: [0.0, 0.0, 0.0, 0.0],
                };

                vertices.push(v);
            }

            let mut tangents: Vec<[f32; 3]> = Vec::default();
            tangents.resize(vertices.len(), [0.0, 0.0, 0.0]);
            let mut bitangents: Vec<[f32; 3]> = Vec::default();
            bitangents.resize(vertices.len(), [0.0, 0.0, 0.0]);

            let mut triangles_included = vec![0; vertices.len()];

            for c in indices.chunks(3) {
                let v0 = vertices[c[0] as usize];
                let v1 = vertices[c[1] as usize];
                let v2 = vertices[c[2] as usize];

                let pos0: cgmath::Vector3<_> = v0.position.into();
                let pos1: cgmath::Vector3<_> = v1.position.into();
                let pos2: cgmath::Vector3<_> = v2.position.into();

                let uv0: cgmath::Vector2<_> = cgmath::Vector2 {
                    x: v0.uv[0],
                    y: v0.uv[1],
                };
                let uv1: cgmath::Vector2<_> = cgmath::Vector2 {
                    x: v1.uv[0],
                    y: v1.uv[1],
                };
                let uv2: cgmath::Vector2<_> = cgmath::Vector2 {
                    x: v2.uv[0],
                    y: v2.uv[1],
                };

                let delta_pos1 = pos1 - pos0;
                let delta_pos2 = pos2 - pos0;

                let delta_uv1 = uv1 - uv0;
                let delta_uv2 = uv2 - uv0;

                let denom = delta_uv1.x * delta_uv2.y - delta_uv1.y * delta_uv2.x;
                if denom.abs() < 1e-8 {
                    continue;
                }
                let r = 1.0 / denom;

                let tangent = (delta_pos1 * delta_uv2.y - delta_pos2 * delta_uv1.y) * r;

                let bitangent = (delta_pos2 * delta_uv1.x - delta_pos1 * delta_uv2.x) * -r;

                tangents[c[0] as usize] =
                    (tangent + cgmath::Vector3::from(tangents[c[0] as usize])).into();
                tangents[c[1] as usize] =
                    (tangent + cgmath::Vector3::from(tangents[c[1] as usize])).into();
                tangents[c[2] as usize] =
                    (tangent + cgmath::Vector3::from(tangents[c[2] as usize])).into();

                bitangents[c[0] as usize] =
                    (bitangent + cgmath::Vector3::from(bitangents[c[0] as usize])).into();

                bitangents[c[1] as usize] =
                    (bitangent + cgmath::Vector3::from(bitangents[c[1] as usize])).into();

                bitangents[c[2] as usize] =
                    (bitangent + cgmath::Vector3::from(bitangents[c[2] as usize])).into();

                // Used to average the tangents/bitangents
                triangles_included[c[0] as usize] += 1;
                triangles_included[c[1] as usize] += 1;
                triangles_included[c[2] as usize] += 1;
            }

            for (i, n) in triangles_included.into_iter().enumerate() {
                let denom = 1.0 / n as f32;
                tangents[i] = (cgmath::Vector3::from(tangents[i]) * denom).into();
                bitangents[i] = (cgmath::Vector3::from(bitangents[i]) * denom).into();
            }

            for i in 0..vertices.len() {
                let v: &mut Vertex = vertices.get_mut(i).unwrap();

                let mut vec_tangent: cgmath::Vector3<f32> = tangents[i].into();
                let vec_bitangent: cgmath::Vector3<f32> = bitangents[i].into();
                let mut vec_normal: cgmath::Vector3<f32> = v.normals.into();
                vec_normal = vec_normal.normalize();

                vec_tangent =
                    vec_tangent - vec_normal * cgmath::Vector3::dot(vec_normal, vec_tangent);

                if vec_tangent.magnitude2() < 1e-8 {
                    vec_tangent = cgmath::Vector3::unit_x();
                } else {
                    vec_tangent = vec_tangent.normalize();
                }

                v.tangent[0] = vec_tangent[0];
                v.tangent[1] = vec_tangent[1];
                v.tangent[2] = vec_tangent[2];

                if cgmath::dot(
                    cgmath::Vector3::cross(vec_normal, vec_tangent),
                    vec_bitangent,
                ) < 0.0
                {
                    v.tangent[3] = -1.0;
                } else {
                    v.tangent[3] = 1.0;
                }

                v.normals = vec_normal.into();
            }

            let mut bytes: Vec<u8> = vec![];

            bytes.extend_from_slice(bytemuck::cast_slice(&vertices));
            bytes.extend_from_slice(bytemuck::cast_slice(&indices));

            let index_byte_offset = vertices.len() * size_of::<Vertex>();
            let total_offset = index_byte_offset + indices.len() * size_of::<u32>();
            number_indices[i] = indices.len() as u32;


            if i == 0 {
                submesh_range[i] = [0, total_offset as u32];

                submesh_index_offset[i] = index_byte_offset as u32;
            }
            else {
                submesh_range[i] = [submesh_range[i - 1][1], (submesh_range[i - 1][1] + total_offset as u32)];

                submesh_index_offset[i] = submesh_index_offset[i - 1] + (index_byte_offset as u32);
            }

            mesh_bin_data.extend_from_slice(bytemuck::cast_slice(&vertices));
            mesh_bin_data.extend_from_slice(bytemuck::cast_slice(&indices));
        }

        let mesh_info = serde_json::json!({"Submesh Ranges": submesh_range, "Index Offsets" : submesh_index_offset, "Index Number": number_indices});

        let _ = std::fs::create_dir_all("scene/meshes");
        let file_path =
            "scene/meshes/".to_string() + &mesh_index.to_string() + &".json".to_string();
        let bin_path = "scene/meshes/".to_string() + &mesh_index.to_string() + &".bin".to_string();
        let _ = std::fs::write(file_path, serde_json::to_string_pretty(&mesh_info).unwrap());

        let _ = std::fs::write(bin_path, bytemuck::cast_slice(&mesh_bin_data));
    }
}

fn process_node(node: &gltf::Node, entity_to_node: &mut HashMap<usize, u32>) -> Entity {
    let mut entity = Entity::default();

    entity_to_node.insert(node.index(), entity.id);

    let node_transform_to_component = |node: &gltf::Node| -> serde_json::Value {
        let t = node.transform();
        let (translation, rotation, scale) = t.decomposed();

        return serde_json::json !( {
            "translation" : translation,
            "rotation" : rotation,
            "scale" : scale,
        });
    };

    entity
        .components
        .insert("Transform".to_string(), node_transform_to_component(node));

    if let Some(mesh) = node.mesh() {
        entity
            .components
            .insert("Mesh".to_string(), serde_json::json!(mesh.index()));
    }

    if let Some(name) = node.name() {
        entity
            .components
            .insert("Name".to_string(), serde_json::json!(name));
    }

    if let Some(cam) = node.camera() {
        let cam_json: serde_json::Value = match cam.projection() {
            gltf::camera::Projection::Orthographic(orthographic) => {
                serde_json::json!({
                        "type" : "Orthographic",
                        "xmag" : orthographic.xmag(),
                        "ymag" : orthographic.ymag(),
                        "zfar" : orthographic.zfar(),
                        "znear" : orthographic.znear(),
                })
            }
            gltf::camera::Projection::Perspective(perspective) => {
                serde_json::json!({
                    "type" : "Perspective",
                    "yfov" : perspective.yfov(),
                    "zfar" : perspective.zfar(),
                    "znear" : perspective.znear(),
                    "aspect_ratio" : perspective.aspect_ratio(),
                })
            }
        };

        entity.components.insert("Camera".to_string(), cam_json);
    }

    if let Some(light) = node.light() {
        let light_json = match light.kind() {
            gltf::khr_lights_punctual::Kind::Directional => {
                serde_json::json!({
                    "type" : "Directional",
                    "color" : light.color(),
                    "intensity" : light.intensity(),
                })
            }
            gltf::khr_lights_punctual::Kind::Point => {
                serde_json::json!({
                        "type" : "Point",
                        "color" : light.color(),
                        "intensity" : light.intensity(),
                        "range" : light.range(),
                })
            }
            gltf::khr_lights_punctual::Kind::Spot {
                inner_cone_angle,
                outer_cone_angle,
            } => {
                serde_json::json!({
                        "type" : "Spot",
                        "color" : light.color(),
                        "intensity" : light.intensity(),
                        "range" : light.range(),
                        "inner_angle" : inner_cone_angle,
                        "outer_angle" : outer_cone_angle,
                })
            }
        };

        entity.components.insert("Light".to_string(), light_json);
    }

    if let Some(extra) = node.extras() {
        let extra_str = extra.get();

        let value: serde_json::Value = serde_json::from_str(extra_str).unwrap_or_default();

        for (k, v) in value.as_object().unwrap_or(&serde_json::Map::new()) {
            entity.components.insert(k.to_string(), v.clone());
        }
    }

    return entity;
}

fn process_images(scene: &gltf::Document, images: &Vec<gltf::image::Data>) {
    for img in scene.images() {
        let img_index = img.index();

        let mut img_data: Vec<u8> = vec![];
        let mut img_json: serde_json::Value;
        let mut width: u32;
        let mut height: u32;

        match img.source() {
            Source::View { view, mime_type } => {
                let image: &gltf::image::Data = images.get(img_index).unwrap();

                assert!(image.pixels.len() > 0);

                img_data = match image.format {
                    gltf::image::Format::R8 => image
                        .pixels
                        .iter()
                        .flat_map(|r| [*r, *r, *r, 255])
                        .collect(),

                    gltf::image::Format::R8G8 => image
                        .pixels
                        .chunks(2)
                        .flat_map(|rg| [rg[0], rg[0], rg[0], rg[1]])
                        .collect(),

                    gltf::image::Format::R8G8B8 => image
                        .pixels
                        .chunks(3)
                        .flat_map(|c| [c[0], c[1], c[2], 255])
                        .collect(),

                    gltf::image::Format::R8G8B8A8 => image.pixels.clone(),

                    gltf::image::Format::R16 => image
                        .pixels
                        .chunks(2)
                        .flat_map(|r| {
                            let v = u16::from_le_bytes([r[0], r[1]]) >> 8;
                            [v as u8, v as u8, v as u8, 255]
                        })
                        .collect(),

                    gltf::image::Format::R16G16 => image
                        .pixels
                        .chunks(4)
                        .flat_map(|c| {
                            let r = u16::from_le_bytes([c[0], c[1]]) >> 8;
                            let g = u16::from_le_bytes([c[2], c[3]]) >> 8;
                            [r as u8, r as u8, r as u8, g as u8]
                        })
                        .collect(),

                    gltf::image::Format::R16G16B16 => image
                        .pixels
                        .chunks(6)
                        .flat_map(|c| {
                            let r = u16::from_le_bytes([c[0], c[1]]) >> 8;
                            let g = u16::from_le_bytes([c[2], c[3]]) >> 8;
                            let b = u16::from_le_bytes([c[4], c[5]]) >> 8;
                            [r as u8, g as u8, b as u8, 255]
                        })
                        .collect(),

                    gltf::image::Format::R16G16B16A16 => image
                        .pixels
                        .chunks(8)
                        .flat_map(|c| {
                            let r = u16::from_le_bytes([c[0], c[1]]) >> 8;
                            let g = u16::from_le_bytes([c[2], c[3]]) >> 8;
                            let b = u16::from_le_bytes([c[4], c[5]]) >> 8;
                            let a = u16::from_le_bytes([c[6], c[7]]) >> 8;
                            [r as u8, g as u8, b as u8, a as u8]
                        })
                        .collect(),

                    gltf::image::Format::R32G32B32FLOAT => {
                        let f: &[f32] = bytemuck::cast_slice(&image.pixels);
                        f.chunks(3)
                            .flat_map(|c| {
                                [
                                    (c[0] * 255.0) as u8,
                                    (c[1] * 255.0) as u8,
                                    (c[2] * 255.0) as u8,
                                    255,
                                ]
                            })
                            .collect()
                    }

                    gltf::image::Format::R32G32B32A32FLOAT => {
                        let f: &[f32] = bytemuck::cast_slice(&image.pixels);
                        f.chunks(4)
                            .flat_map(|c| {
                                [
                                    (c[0] * 255.0) as u8,
                                    (c[1] * 255.0) as u8,
                                    (c[2] * 255.0) as u8,
                                    (c[3] * 255.0) as u8,
                                ]
                            })
                            .collect()
                    }
                };

                width = image.width;
                height = image.height;
            }

            Source::Uri { uri, mime_type } => {
                let dyn_img = image::open(uri).unwrap();

                let rgba_img = dyn_img.to_rgba8();

                width = rgba_img.width();
                height = rgba_img.height();
                img_data = rgba_img.into_raw();
            }
        }

        let value = serde_json::json!({"width" : width, "height" : height});

        let _ = std::fs::create_dir_all("scene/images");
        let file_path = "scene/images/".to_string() + &img_index.to_string() + &".json".to_string();
        let bin_path = "scene/images/".to_string() + &img_index.to_string() + &".bin".to_string();
        let _ = std::fs::write(file_path, serde_json::to_string_pretty(&value).unwrap());

        let _ = std::fs::write(bin_path, bytemuck::cast_slice(&img_data));
    }
}

fn process_scene(scene: &gltf::Document) {
    let mut entities: Vec<Entity> = Vec::default();

    let mut node_index_to_entity_id: HashMap<usize, u32> = HashMap::default();

    for scene in scene.scenes() {
        for node in scene.nodes() {
            let e = process_node(&node, &mut node_index_to_entity_id);

            entities.push(e);

            for child_node in node.children() {
                entities.push(process_node(&child_node, &mut node_index_to_entity_id));
            }
        }
    }

    let mut parent_child_pairs = Vec::new();

    for scene in scene.scenes() {
        for node in scene.nodes() {
            let e_id = *node_index_to_entity_id.get(&node.index()).unwrap();
            for child in node.children() {
                let child_entity_id = *node_index_to_entity_id.get(&child.index()).unwrap();
                parent_child_pairs.push((e_id, child_entity_id));
            }
        }
    }

    for (parent_id, child_id) in parent_child_pairs {
        entities[parent_id as usize].children.push(child_id);
        entities[child_id as usize].parent = parent_id as i32;
    }

    let _ = std::fs::create_dir("scene");

    let _ = std::fs::write(
        "scene/scene.json",
        serde_json::to_string_pretty(&entities).unwrap(),
    );
}
