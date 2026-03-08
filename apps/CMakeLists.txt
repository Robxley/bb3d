# --- Applications ---

file(GLOB APP_SOURCES "*.cpp")

foreach(APP_SOURCE ${APP_SOURCES})
    get_filename_component(APP_NAME ${APP_SOURCE} NAME_WE)
    
    # Skip bb3d_editor if the editor is disabled
    if(APP_NAME STREQUAL "bb3d_editor" AND NOT BB3D_ENABLE_EDITOR)
        continue()
    endif()
    
    add_executable(${APP_NAME} ${APP_SOURCE})
    target_link_libraries(${APP_NAME} PRIVATE biobazard3d)
    
    # Explicitly propagate BB3D_ENABLE_EDITOR to the editor app source files
    # (required for the #if defined(BB3D_ENABLE_EDITOR) guard around main())
    if(APP_NAME STREQUAL "bb3d_editor" AND BB3D_ENABLE_EDITOR)
        target_compile_definitions(${APP_NAME} PRIVATE BB3D_ENABLE_EDITOR)
    endif()
    
    # All apps run from the common bin directory (where assets/ is deployed)
    set_target_properties(${APP_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    
    add_dependencies(${APP_NAME} DeployAssets)
endforeach()
