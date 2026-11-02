add_subdirectory(union)
add_subdirectory(imgui)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE union imgui)
