#pragma once

#include <iostream>

#define VK_ERROR(result, message)                                              \
  {                                                                            \
    if (result != VK_SUCCESS) {                                                \
      std::cerr << "Error: " << message << " : " << result << "\n";            \
      abort();                                                                 \
    }                                                                          \
  }