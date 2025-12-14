#pragma once

#include "game/ecs/vox_ecs.h"
#include "game/plugin.h"

#include "nlohmann/json.hpp"
#include "platform/math/math.h"

using json = nlohmann::json;

template <typename T> void to_json(json &j, const Vec3<T> &v) {

  j = json{{v.x, v.y, v.z}};
}

template <typename T> void from_json(const json &j, Vec3<T> &v) {

  v = {j[0], j[1], j[2]};
}

template <typename T> void to_json(json &j, const Quat<T> &v) {

  j = json{{v.x, v.y, v.z, v.w}};
}

template <typename T> void from_json(const json &j, Quat<T> &v) {

  v = {j[0], j[1], j[2], j[3]};
}

struct Transform {

  Vec3<float> translation = Vec3<float>(0.0f, 0.0f, 0.0f);
  Quat<float> rotation = Quat<float>::identity();
  Vec3<float> scale = Vec3<float>(1.f, 1.f, 1.f);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Transform, translation, rotation, scale);

struct Parent {
  vecs::Entity id;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Parent, id)

struct Name {

  std::string name;
};

inline void to_json(nlohmann::json &j, const Name &n) { j = n.name; }

inline void from_json(const nlohmann::json &j, Name &n) {
  n.name = j.get<std::string>();
}

/// Contains default compoents and plugins
class DefaultComponentsPlugin : public IPlugin {

  void build(game::Game &game) override;
};