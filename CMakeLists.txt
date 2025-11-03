cmake_minimum_required(VERSION 3.10.0)
project(Vulkaner VERSION 0.1.0 LANGUAGES C CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)






find_package(Vulkan REQUIRED)

file(GLOB_RECURSE MY_HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
file(GLOB_RECURSE MY_CODE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")




add_subdirectory(deps/glfw)


#Volk for loading vulkan pointers

if (WIN32)
   set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XCB_KHR)


endif()


add_subdirectory(deps/volk)


add_executable(${PROJECT_NAME} ${MY_HEADER_FILES} ${MY_CODE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})


target_include_directories(${PROJECT_NAME} PRIVATE "src/")

target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARIES} glfw volk slang)


add_compile_definitions(VULKAN_USE_VALIDATION_LAYERS)

set(SHADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_PATH="${SHADER_PATH}")



if (MSVC)
    target_compile_options(${PROJECT_NAME}  PRIVATE /O2 /Ob2 /DNDEBUG) 
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME}  PRIVATE -O3 -DNDEBUG)

endif()

