# --- Applications ---

add_executable(bb3d_hello_world bb3d_hello_world/main.cpp)
target_link_libraries(bb3d_hello_world PRIVATE biobazard3d)
set_target_properties(bb3d_hello_world PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
add_dependencies(bb3d_hello_world DeployAssets)

if(BB3D_ENABLE_EDITOR)
    add_executable(bb3d_editor bb3d_editor/main.cpp)
    target_link_libraries(bb3d_editor PRIVATE biobazard3d)
    target_compile_definitions(bb3d_editor PRIVATE BB3D_ENABLE_EDITOR)
    set_target_properties(bb3d_editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    add_dependencies(bb3d_editor DeployAssets)
endif()

# AstroBazard Concept App
add_executable(astro_bazard 
    astro_bazard/main.cpp
    astro_bazard/OrbitalGravity.cpp
    astro_bazard/SpaceshipController.cpp
    astro_bazard/SmartCamera.cpp
)
target_link_libraries(astro_bazard PRIVATE biobazard3d)
set_target_properties(astro_bazard PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
add_dependencies(astro_bazard DeployAssets)
