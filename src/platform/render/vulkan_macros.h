#pragma once

#include <iostream>

#define VK_ERROR(result, message)                                              \
  do {                                                                         \
    VkResult _vk_err_res = (result);                                           \
    if (_vk_err_res != VK_SUCCESS) {                                           \
      std::cerr << "Error: " << message << " : " << _vk_err_res << "\n";       \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#define VK_CHECK(result, message)                                              \
  do {                                                                         \
    VkResult _vk_check_res = (result);                                         \
    if (_vk_check_res != VK_SUCCESS) {                                         \
      std::cerr << "Error: " << message << " : " << _vk_check_res << "\n";     \
    }                                                                          \
  } while (0)