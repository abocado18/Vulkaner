#version 450

// ===== Vertex input =====
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec2 in_texcoord0;
layout(location = 4) in vec2 in_texcoord1;

// ===== Camera uniform buffer =====
layout(set = 0, binding = 0) uniform CamData
{
    mat4 proj;
    mat4 view;
} cam;

// ===== Transform uniform buffer =====
layout(set = 1, binding = 0) uniform Transform
{
    mat4 model;
} transform;



void main()
{
    gl_Position = cam.proj * cam.view * transform.model * vec4(in_position, 1.0);

    
}
