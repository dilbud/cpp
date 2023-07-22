
if (TEST_SRT)

else()
    add_executable(main main.cpp)
    find_package(owt_web_transport REQUIRED)
    find_package(uv REQUIRED)
    target_link_libraries(main owt_web_transport uv peer)
endif(TEST_SRT)