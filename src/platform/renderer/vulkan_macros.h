#pragma once

#include <iostream>


#define VK_ERROR(result)                                                       \
  {                                                                            \
    if (result != VK_SUCCESS) {                                                \
      std::cerr << "Error: " << result << "\n";                                \
      abort();                                                                 \
    }                                                                          \
  }