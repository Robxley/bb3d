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

FetchContent_MakeAvailable(spdlog nlohmann_json tracy SDL VulkanMemoryAllocator glm)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found! Please install it.")
endif()


# --- Engine Library (Automatic Source Discovery) ---
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(biobazard3d ${ENGINE_SOURCES})

target_include_directories(biobazard3d PUBLIC 
    include
)

# Note: nlohmann_json is header-only, spdlog needs linking
target_link_libraries(biobazard3d PUBLIC 
    spdlog::spdlog 
    nlohmann_json::nlohmann_json
    SDL3::SDL3
    Vulkan::Vulkan
    glm::glm
    VulkanMemoryAllocator
)

# Apply strict warnings only to our target, not dependencies
target_compile_options(biobazard3d PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
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

message(STATUS "biobazard3d project configured for C++20 with strict warnings.")
message(STATUS "To build, run: cmake --build build")
