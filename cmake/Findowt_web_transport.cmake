# include(FindPackageHandleStandardArgs)
# find_package_handle_standard_args(  owt_web_transport 
#                                     REQUIRED_VARS
#                                     ${CMAKE_SOURCE_DIR}/extern/lib/owt_web_transport/libowt_web_transport.so
#                                     ${CMAKE_SOURCE_DIR}/extern/header/owt_web_transport)


add_library( owt_web_transport
             SHARED
             IMPORTED )

set_target_properties( owt_web_transport
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/extern/lib/owt_web_transport/libowt_web_transport.so )

target_include_directories( owt_web_transport
                            INTERFACE
                            ${CMAKE_SOURCE_DIR}/extern/header/owt_web_transport )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(owt_web_transport DEFAULT_MSG)