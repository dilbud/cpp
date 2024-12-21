add_executable(app app.cpp)
# target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ThreadPool.cpp )
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET 
    libavdevice 
    libavfilter 
    libavformat 
    libavcodec 
    libswresample 
    libswscale 
    libavutil
)

pkg_check_modules(LIB_EVENT2 REQUIRED IMPORTED_TARGET
    libevent_core
    libevent
    libevent_pthreads
    libevent_extra
    libevent_openssl
)

include_directories(${LIBEVENT_INCLUDE_DIRS})

target_link_libraries(app PkgConfig::LIBAV PkgConfig::LIB_EVENT2 ${LIBEVENT_LIBRARIES})