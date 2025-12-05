#pragma once
#include "platform/math/math.h"
#include "platform/render/resources.h"

struct Transform {

  Vec3<float> translation = Vec3<float>(0.0f, 0.0f, 0.0f);
  Vec3<float> rotation = Vec3<float>(0.0f, 0.0f, 0.0f);
  Vec3<float> scale = Vec3<float>(1.f, 1.f, 1.f);
};


struct GpuTransform
{
  BufferHandle transform_handle;
};