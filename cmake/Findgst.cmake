find_package(PkgConfig REQUIRED)
# pkg_check_modules(gtk3 REQUIRED IMPORTED_TARGET gtk+-3.0)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

# pkg_check_modules(gtk3 REQUIRED IMPORTED_TARGET gtk+-3.0)
# pkg_check_modules(gst REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
# pkg_check_modules(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
# pkg_check_modules(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
# pkg_check_modules(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

pkg_check_modules(gst REQUIRED IMPORTED_TARGET
gstreamer-1.0>=1.4
gstreamer-sdp-1.0>=1.4
gstreamer-app-1.0>=1.4 
gstreamer-video-1.0>=1.4)

# pkg_check_modules(gstreamer REQUIRED gstreamer)
# message(STATUS "${gstreamer_LIBRARIES}")

# add_library( gst SHARED IMPORTED )

# target_link_libraries(   gst
#     # gtk3_LIBRARY_DIRS
#     INTERFACE gstreamer_LIBRARIES
#     INTERFACE gstreamer-sdp_LIBRARIES
#     INTERFACE gstreamer-app_LIBRARIES
#     INTERFACE gstreamer-video_LIBRARIES
# )

# target_include_directories( gst 
#     # gtk3_INCLUDE_DIRS
#     INTERFACE gstreamer_INCLUDE_DIRS
#     INTERFACE gstreamer-sdp_INCLUDE_DIRS
#     INTERFACE gstreamer-app_INCLUDE_DIRS
#     INTERFACE gstreamer-video_INCLUDE_DIRS
# )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gst DEFAULT_MSG)