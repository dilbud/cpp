find_package(PkgConfig REQUIRED)
pkg_check_modules(ffmpeg REQUIRED IMPORTED_TARGET
    libavdevice
    libavfilter
    libavformat
    libavcodec
    libswresample
    libswscale
    libavutil
)

# message(STATUS "${ffmpeg_INCLUDE_DIRS}")
# message(STATUS "${ffmpeg_LINK_LIBRARIES}")

# add_library( ffmpeg
#              SHARED
#              IMPORTED )

# set_target_properties( ffmpeg
#                        PROPERTIES IMPORTED_LOCATION
#                        ${LIBAV_LINK_LIBRARIES} )

# target_include_directories( ffmpeg
#                             INTERFACE
#                             ${LIBAV_INCLUDE_DIRS} )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ffmpeg DEFAULT_MSG)