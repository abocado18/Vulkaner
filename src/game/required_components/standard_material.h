#pragma once
#include "game/ecs/vox_ecs.h"
#include "platform/math/math.h"

struct StandardMaterial
{
    Vec3<float> albedo_color;
    Vec3<float> normal_dir;

    vecs::Entity albedo_texture = vecs::NO_ENTITY;

    float metallic;
    float roughness;
    float ao;
};