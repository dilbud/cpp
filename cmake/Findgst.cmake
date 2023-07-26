find_package(PkgConfig REQUIRED)

pkg_check_modules(gst
    REQUIRED 
    IMPORTED_TARGET 
    GLOBAL 
    gstreamer-1.0>=1.4
    gstreamer-sdp-1.0>=1.4
    gstreamer-app-1.0>=1.4 
    gstreamer-video-1.0>=1.4
)

# message(STATUS "${gst_LINK_LIBRARIES}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gst DEFAULT_MSG)