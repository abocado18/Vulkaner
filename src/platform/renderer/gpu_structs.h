#pragma once
#include <array>
#include <cinttypes>
#include <cstdint>

// Structs for GPU Data

struct alignas(16) Vector3 {

  Vector3() = default;

  Vector3(float x, float y, float z) : x(x), y(y), z(z) {};

  float x;
  float y;
  float z;
};

static_assert(sizeof(Vector3) == 16);

struct alignas(16) Vector4 {

  Vector4() = default;

  Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {};

  float x;
  float y;
  float z;
  float w;
};

static_assert(sizeof(Vector4) == 16);

struct alignas(8) Vector2 {

  Vector2() = default;

  Vector2(float x, float y) : x(x), y(y) {};

  float x;
  float y;
};

static_assert(sizeof(Vector2) == 8);

struct alignas(16) IVector3 {

  IVector3() = default;

  IVector3(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {};

  int32_t x;
  int32_t y;
  int32_t z;
};

static_assert(sizeof(IVector3) == 16);

struct alignas(16) IVector4 {

  IVector4() = default;

  IVector4(int32_t x, int32_t y, int32_t z, int32_t w)
      : x(x), y(y), z(z), w(w) {};

  int32_t x;
  int32_t y;
  int32_t z;
  int32_t w;
};

static_assert(sizeof(IVector4) == 16);

struct alignas(8) IVector2 {

  IVector2() = default;

  IVector2(int32_t x, int32_t y) : x(x), y(y) {};

  int32_t x;
  int32_t y;
};

static_assert(sizeof(IVector2) == 8);

struct alignas(64) Matrix {

  Matrix() = default;

  std::array<float, 16> mat;
};

static_assert(sizeof(Matrix) == 64);

struct alignas(64) IMatrix {

  IMatrix() = default;

  std::array<int32_t, 16> mat;
};

static_assert(sizeof(IMatrix) == 64);