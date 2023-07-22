# include(FetchContent)

# FetchContent_Declare(   libsrt-base
#                         GIT_REPOSITORY https://github.com/Haivision/srt.git
#                         GIT_TAG v1.5.2
#                         GIT_SHALLOW 1 )

# FetchContent_GetProperties(libsrt-base)
# FetchContent_MakeAvailable(libsrt-base)


# # if(NOT libsrt_POPULATED)
# #     FetchContent_Populate(libsrt)
# #     add_subdirectory(   ${libsrt_SOURCE_DIR}
# #                         ${libsrt_BINARY_DIR}
# #                         EXCLUDE_FROM_ALL )
# # endif()

# add_library( srt_lib
#              SHARED
#              IMPORTED )

# target_link_libraries( srt_lib INTERFACE libsrt-base)

# target_include_directories( srt_lib
#                             INTERFACE
#                             ${libsrt-base_SOURCE_DIR}/srtcore INTERFACE ${libsrt-base_SOURCE_DIR}/common )