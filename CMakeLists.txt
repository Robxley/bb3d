cmake_minimum_required(VERSION 3.20)

project(biobazard3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Debug
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# --- Dependencies ---
include(FetchContent)

# spdlog for logging
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.17.0
)

# nlohmann_json for configuration
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.12.0
)

# Tracy for profiling
FetchContent_Declare(
  tracy
  GIT_REPOSITORY https://github.com/wolfpld/tracy.git
  GIT_TAG        v0.13.1
)

# SDL3 for windowing and input
FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.0
)

# Vulkan Memory Allocator (VMA)
FetchContent_Declare(
  VulkanMemoryAllocator
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG        v3.3.0
)

# GLM for mathematics
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.3
)

# stb for image loading
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
)

# fastgltf for 3D model loading
FetchContent_Declare(
  fastgltf
  GIT_REPOSITORY https://github.com/spnda/fastgltf.git
  GIT_TAG        v0.8.0
)

# tinyobjloader for .obj model loading
FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG        release
)

# EnTT for ECS
FetchContent_Declare(
  entt
  GIT_REPOSITORY https://github.com/skypjack/entt.git
  GIT_TAG        v3.13.2
)

FetchContent_MakeAvailable(spdlog nlohmann_json tracy SDL VulkanMemoryAllocator glm stb fastgltf tinyobjloader entt)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found! Please install it.")
endif()

# Find glslc shader compiler
find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS ${Vulkan_SDK}/bin
          $ENV{VULKAN_SDK}/bin
)

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found! Please ensure Vulkan SDK bin directory is in PATH.")
endif()

# Function to compile shaders
function(compile_shader SHADER_SOURCE SHADER_OUTPUT)
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER_SOURCE} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling ${SHADER_SOURCE} to ${SHADER_OUTPUT}"
    )
endfunction()

# Compile default shaders
set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
set(SHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/assets/shaders")
file(MAKE_DIRECTORY ${SHADER_BUILD_DIR})

set(SHADERS
    "${SHADER_DIR}/triangle.vert"
    "${SHADER_DIR}/triangle.frag"
    "${SHADER_DIR}/triangle_buffer.vert"
    "${SHADER_DIR}/simple_3d.vert"
    "${SHADER_DIR}/simple_3d.frag"
    "${SHADER_DIR}/texture_cube.vert"
    "${SHADER_DIR}/texture_cube.frag"
    "${SHADER_DIR}/textured_mesh.vert"
    "${SHADER_DIR}/textured_mesh.frag"
)

set(SPV_SHADERS "")
foreach(SHADER ${SHADERS})
    get_filename_component(FILENAME ${SHADER} NAME)
    set(SPV_OUTPUT "${SHADER_BUILD_DIR}/${FILENAME}.spv")
    compile_shader(${SHADER} ${SPV_OUTPUT})
    list(APPEND SPV_SHADERS ${SPV_OUTPUT})
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${SPV_SHADERS})


# --- Engine Library (Automatic Source Discovery) ---
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(biobazard3d ${ENGINE_SOURCES})

target_include_directories(biobazard3d PUBLIC 
    include
    ${stb_SOURCE_DIR}
)

# Note: nlohmann_json is header-only, spdlog needs linking
target_link_libraries(biobazard3d PUBLIC 
    spdlog::spdlog 
    nlohmann_json::nlohmann_json
    SDL3::SDL3
    Vulkan::Vulkan
    glm::glm
    VulkanMemoryAllocator
    fastgltf::fastgltf
    tinyobjloader
    EnTT::EnTT
)

# Apply strict warnings only to our target, not dependencies
target_compile_options(biobazard3d PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# Enable profiling for the engine based on a CMake option
option(BB3D_ENABLE_PROFILING "Enable Tracy profiling" ON)
if(BB3D_ENABLE_PROFILING)
    target_compile_definitions(biobazard3d PUBLIC BB_PROFILE)
    target_link_libraries(biobazard3d PUBLIC TracyClient)
endif()


# --- Unit Tests ---
add_executable(unit_test_00_infrastructure
    tests/unit_test_00_infrastructure.cpp
)
target_link_libraries(unit_test_00_infrastructure PRIVATE biobazard3d)


add_executable(unit_test_01_window
    tests/unit_test_01_window.cpp
)
target_link_libraries(unit_test_01_window PRIVATE biobazard3d)

# Copy assets to build directory
add_custom_command(TARGET unit_test_01_window POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/engine_config.json
    $<TARGET_FILE_DIR:unit_test_01_window>/engine_config.json
)

add_executable(unit_test_02_vulkan_init
    tests/unit_test_02_vulkan_init.cpp
)
target_link_libraries(unit_test_02_vulkan_init PRIVATE biobazard3d)

add_executable(unit_test_03_swapchain
    tests/unit_test_03_swapchain.cpp
)
target_link_libraries(unit_test_03_swapchain PRIVATE biobazard3d)

add_executable(unit_test_04_hello_triangle
    tests/unit_test_04_hello_triangle.cpp
)
target_link_libraries(unit_test_04_hello_triangle PRIVATE biobazard3d)
add_dependencies(unit_test_04_hello_triangle CompileShaders)

# Copy shaders to executable directory
add_custom_command(TARGET unit_test_04_hello_triangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_BINARY_DIR}/assets/shaders
    $<TARGET_FILE_DIR:unit_test_04_hello_triangle>/assets/shaders
)

add_executable(unit_test_05_vertex_buffer
    tests/unit_test_05_vertex_buffer.cpp
)
target_link_libraries(unit_test_05_vertex_buffer PRIVATE biobazard3d)
add_dependencies(unit_test_05_vertex_buffer CompileShaders)

add_custom_command(TARGET unit_test_05_vertex_buffer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_BINARY_DIR}/assets/shaders
    $<TARGET_FILE_DIR:unit_test_05_vertex_buffer>/assets/shaders
)

add_executable(unit_test_06_rotating_cube
    tests/unit_test_06_rotating_cube.cpp
)
target_link_libraries(unit_test_06_rotating_cube PRIVATE biobazard3d)
add_dependencies(unit_test_06_rotating_cube CompileShaders)

add_custom_command(TARGET unit_test_06_rotating_cube POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_BINARY_DIR}/assets/shaders
    $<TARGET_FILE_DIR:unit_test_06_rotating_cube>/assets/shaders
)

add_executable(unit_test_07_texture_cube
    tests/unit_test_07_texture_cube.cpp
)
target_link_libraries(unit_test_07_texture_cube PRIVATE biobazard3d)
add_dependencies(unit_test_07_texture_cube CompileShaders)

add_custom_command(TARGET unit_test_07_texture_cube POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_BINARY_DIR}/assets/shaders
    $<TARGET_FILE_DIR:unit_test_07_texture_cube>/assets/shaders
)

add_executable(unit_test_08_core_systems
    tests/unit_test_08_core_systems.cpp
)
target_link_libraries(unit_test_08_core_systems PRIVATE biobazard3d)

# --- Unit Test 10: OBJ Loader (House) ---
add_executable(unit_test_10_obj_loader
    tests/unit_test_10_obj_loader.cpp
)
target_link_libraries(unit_test_10_obj_loader PRIVATE biobazard3d)
add_dependencies(unit_test_10_obj_loader CompileShaders)

add_custom_command(TARGET unit_test_10_obj_loader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_BINARY_DIR}/assets/shaders
    $<TARGET_FILE_DIR:unit_test_10_obj_loader>/assets/shaders
)
add_custom_command(TARGET unit_test_10_obj_loader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:unit_test_10_obj_loader>/assets
)

# --- Unit Test 11: GLTF Loader (Box) ---
add_executable(unit_test_11_gltf_loader
    tests/unit_test_11_gltf_loader.cpp
)
target_link_libraries(unit_test_11_gltf_loader PRIVATE biobazard3d)
add_dependencies(unit_test_11_gltf_loader CompileShaders)

add_custom_command(TARGET unit_test_11_gltf_loader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_BINARY_DIR}/assets/shaders
    $<TARGET_FILE_DIR:unit_test_11_gltf_loader>/assets/shaders
)
add_custom_command(TARGET unit_test_11_gltf_loader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:unit_test_11_gltf_loader>/assets
)

# --- Unit Test 12: ECS & Scene ---
add_executable(unit_test_12_ecs
    tests/unit_test_12_ecs.cpp
)
target_link_libraries(unit_test_12_ecs PRIVATE biobazard3d)

# --- Unit Test 13: Camera ---
add_executable(unit_test_13_camera
    tests/unit_test_13_camera.cpp
)
target_link_libraries(unit_test_13_camera PRIVATE biobazard3d)

# --- Unit Test 14: Interactive Cameras ---
add_executable(unit_test_14_interactive_cameras
    tests/unit_test_14_interactive_cameras.cpp
)
target_link_libraries(unit_test_14_interactive_cameras PRIVATE biobazard3d)
add_dependencies(unit_test_14_interactive_cameras CompileShaders)

add_custom_command(TARGET unit_test_14_interactive_cameras POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_BINARY_DIR}/assets/shaders
    $<TARGET_FILE_DIR:unit_test_14_interactive_cameras>/assets/shaders
)
add_custom_command(TARGET unit_test_14_interactive_cameras POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets/textures
    $<TARGET_FILE_DIR:unit_test_14_interactive_cameras>/assets/textures
)

add_executable(unit_test_09_resource_manager
    tests/unit_test_09_resource_manager.cpp
)
target_link_libraries(unit_test_09_resource_manager PRIVATE biobazard3d)

add_custom_command(TARGET unit_test_09_resource_manager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:unit_test_09_resource_manager>/assets
)

message(STATUS "biobazard3d project configured for C++20 with strict warnings.")
message(STATUS "To build, run: cmake --build build")
