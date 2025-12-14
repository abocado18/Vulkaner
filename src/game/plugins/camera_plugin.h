#pragma once
#include "game/plugin.h"
#include "nlohmann/json.hpp"
#include <string>

enum class CameraType { PERSPECTIVE, ORTHOGRAPHIC };

struct Camera {

  CameraType type{};
  float aspct_ratio;
  float fov;
  float z_near;
  float z_far;
};

inline void to_json(nlohmann::json &j, const Camera &c) {
  j["aspect_ratio"] = c.aspct_ratio;
  j["yfov"] = c.fov;
  j["znear"] = c.z_near;
  j["zfar"] = c.z_far;
};

inline void from_json(const nlohmann::json &j, Camera &c) {

  std::string type = j["type"].get<std::string>();

  if (type == "Perspective") {
    c.type = CameraType::PERSPECTIVE;
  } else if (type == "Orthographic") {
    c.type = CameraType::ORTHOGRAPHIC;
  }

  c.fov = j["yfov"].get<float>();
  c.aspct_ratio = j["aspect_ratio"].get<float>();
  c.z_near = j["znear"].get<float>();
  c.z_far = j["zfar"].get<float>();
}

struct CameraPlugin : IPlugin {

  void build(game::Game &game) override;
};