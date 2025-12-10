use std::collections::HashMap;
use std::env;
use std::sync::atomic::AtomicU32;
use std::sync::atomic::Ordering;
use std::vec;

use anyhow::Ok;
use bytemuck::Pod;
use bytemuck::Zeroable;
use gltf::Gltf;
use serde::Deserialize;
use serde::Serialize;

#[derive(Serialize, Deserialize)]
struct Entity {
    id: u32,
    children: Vec<u32>,
    parent: i32,
    components: HashMap<String, serde_json::Value>,
}

#[repr(C)]
#[derive(Clone, Copy, Pod, Zeroable)]
struct Vertex {
    position: [f32; 3],
    _pad0: u32,
    normal: [f32; 3],
    _pad1: u32,
    color: [f32; 4],
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

    process_meshes(&file, &buffers);

    return Ok(());
}

fn process_meshes(scene: &gltf::Document, buffers: &Vec<gltf::buffer::Data>) {
    for mesh in scene.meshes() {
        let mesh_index = mesh.index();

        for primitive in mesh.primitives() {
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
                    normal: *normals.get(i).unwrap_or(&[0.0, 0.0, 0.0]),
                    _pad1: 0,
                    color: *color.get(i).unwrap_or(&[1.0, 1.0, 1.0, 1.0]),
                    uv: [
                        uv0.get(i).unwrap_or(&[0.0, 0.0])[0],
                        uv0.get(i).unwrap_or(&[0.0, 0.0])[1],
                        uv1.get(i).unwrap_or(&[0.0, 0.0])[0],
                        uv1.get(i).unwrap_or(&[0.0, 0.0])[1],
                    ],
                };

                vertices.push(v);
            }

            let mut bytes: Vec<u8> = vec![];

            bytes.extend_from_slice(bytemuck::cast_slice(&vertices));
            bytes.extend_from_slice(bytemuck::cast_slice(&indices));

            let index_byte_offset = vertices.len() * size_of::<Vertex>();
            let number_of_vertices = vertices.len();
            let number_of_indices = indices.len();

            let mesh_info = serde_json::json!({"Index Byte Offset": index_byte_offset, "Vertex Number" : number_of_vertices, "Index Number": number_of_indices});

            let _ = std::fs::create_dir_all("scene/meshes");
            let file_path =
                "scene/meshes/".to_string() + &mesh_index.to_string() + &".json".to_string();
            let bin_path =
                "scene/meshes/".to_string() + &mesh_index.to_string() + &".bin".to_string();
            let _ = std::fs::write(file_path, serde_json::to_string_pretty(&mesh_info).unwrap());

            let _ = std::fs::write(bin_path, bytemuck::cast_slice(&bytes));
        }
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

    if let Some(extra) = node.extras() {
        let extra_str = extra.get();

        let value: serde_json::Value = serde_json::from_str(extra_str).unwrap_or_default();

        for (k, v) in value.as_object().unwrap_or(&serde_json::Map::new()) {
            entity.components.insert(k.to_string(), v.clone());
        }
    }

    return entity;
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
