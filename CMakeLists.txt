cmake_minimum_required(VERSION 3.10.0)
project(Vulkaner VERSION 0.1.0 LANGUAGES C CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)



option(PRODUCTION_BUILD "Turn on when used for finished product" OFF)


find_package(Vulkan REQUIRED)

file(GLOB_RECURSE MY_HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
file(GLOB_RECURSE MY_CODE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")






add_subdirectory(deps/glfw)

add_subdirectory(deps/vk-bootstrap)

#Volk for loading vulkan pointers

if(WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XCB_KHR)


endif()


add_subdirectory(deps/volk)

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(deps/json)



add_executable(${PROJECT_NAME} ${MY_HEADER_FILES} ${IMGUI_HEADER_FILES} ${MY_CODE_FILES} ${IMGUI_CODE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})


target_include_directories(${PROJECT_NAME} PRIVATE "src/")



target_compile_definitions(${PROJECT_NAME} PUBLIC VK_NO_PROTOTYPES) #Use Volk 


#Setup ImGui
set(imgui_external_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui")


add_library(imgui
    ${imgui_external_SOURCE_DIR}/imgui.cpp
    ${imgui_external_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_external_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_external_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_external_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_external_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_external_SOURCE_DIR}/backends/imgui_impl_glfw.cpp

)

target_include_directories(imgui PUBLIC
    ${imgui_external_SOURCE_DIR}
    ${imgui_external_SOURCE_DIR}/backends
   
)

target_include_directories(imgui PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(imgui PUBLIC glfw volk)

target_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)


target_include_directories(${PROJECT_NAME} PRIVATE deps/imgui/)
target_include_directories(${PROJECT_NAME} PRIVATE deps/imgui/backends)


target_include_directories(${PROJECT_NAME} PRIVATE deps/assimp/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/deps/assimp/include)


target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARIES} glfw volk vk-bootstrap::vk-bootstrap imgui nlohmann_json::nlohmann_json)


add_compile_definitions(VULKAN_USE_VALIDATION_LAYERS)

set(SHADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_PATH="${SHADER_PATH}")

set(ASSET_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets")

target_compile_definitions(${PROJECT_NAME} PRIVATE ASSET_PATH="${ASSET_PATH}")


if(PRODUCTION_BUILD)

    message("Is Finished Production Build")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PRODUCTION_BUILD)

else()

    message("Is not finished Production Build")

    include(cmake/FetchSlang.cmake)

    target_link_libraries(${PROJECT_NAME} PRIVATE slang)

    target_copy_slang_binaries(${PROJECT_NAME})



endif()



if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /O2 /Ob2 /DNDEBUG)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -O3 -DNDEBUG)

endif()

