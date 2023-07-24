# include(FetchContent)

# FetchContent_Declare(   libuv
#                         GIT_REPOSITORY https://github.com/libuv/libuv.git
#                         GIT_TAG v1.46.0
#                         GIT_SHALLOW 1 )

# FetchContent_GetProperties(libuv)

# if(NOT libuv_POPULATED)
#     FetchContent_Populate(libuv)
#     add_subdirectory(   ${libuv_SOURCE_DIR}
#                         ${libuv_BINARY_DIR}
#                         EXCLUDE_FROM_ALL )
# endif()

add_library( uv
             SHARED
             IMPORTED )

set_target_properties( uv
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/extern/uv/build/libuv.so )

target_include_directories( uv
                            INTERFACE ${CMAKE_SOURCE_DIR}/extern/uv/include )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(uv DEFAULT_MSG)