add_library(peer peer.cpp)
find_package(uv REQUIRED)
target_link_libraries(peer PRIVATE uv)
target_include_directories(peer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})