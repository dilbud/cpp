find_package(Threads REQUIRED)
add_executable(main_srt main_srt.cpp)
target_link_libraries(main_srt SRTNet ${CMAKE_THREAD_LIBS_INIT})