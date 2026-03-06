# --- Applications ---

file(GLOB APP_SOURCES "*.cpp")

foreach(APP_SOURCE ${APP_SOURCES})
    get_filename_component(APP_NAME ${APP_SOURCE} NAME_WE)
    
    add_executable(${APP_NAME} ${APP_SOURCE})
    target_link_libraries(${APP_NAME} PRIVATE biobazard3d)
    
    # All apps run from the common bin directory (where assets/ is deployed)
    set_target_properties(${APP_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    
    add_dependencies(${APP_NAME} DeployAssets)
endforeach()
