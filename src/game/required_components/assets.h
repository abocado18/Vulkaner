#pragma once

#include <cinttypes>
#include <unordered_map>

template <typename T> struct Assets {
  std::unordered_map<size_t, T> values;

  T &operator[](size_t idx) { return values[idx]; }
};