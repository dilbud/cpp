add_library( srt
             SHARED
             IMPORTED )

set_target_properties( srt
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/extern/srt/libsrt.so )

target_include_directories( srt
                            INTERFACE ${CMAKE_SOURCE_DIR}/extern/srt
                            INTERFACE ${CMAKE_SOURCE_DIR}/extern/srt/common
                            INTERFACE ${CMAKE_SOURCE_DIR}/extern/srt/srtcore )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(srt DEFAULT_MSG)