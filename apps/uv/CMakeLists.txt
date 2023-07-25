add_executable(main_uv main_uv.cpp)
find_package(uv REQUIRED)
target_link_libraries(main_uv uv peer)