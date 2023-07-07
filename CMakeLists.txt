cmake_minimum_required(VERSION 3.0.0)
project(hello VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


message("CMAKE_PROJECT_NAME = ${CMAKE_PROJECT_NAME}")
message("PROJECT_NAME = ${PROJECT_NAME}")

# The compiled library code is here
# add_subdirectory(src)

# The executable code is here
# add_subdirectory(apps)



# include(CTest)
# enable_testing()


add_executable(hello main.cpp)

add_library( owt_web_transport
             SHARED
             IMPORTED )

set_target_properties( owt_web_transport
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_CURRENT_SOURCE_DIR}/lib/libowt_web_transport.so )

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/api)

target_link_libraries( ${PROJECT_NAME} owt_web_transport )

# find_package(Doxygen)
#   if(Doxygen_FOUND)
#     add_subdirectory(docs)
#   else()
#     message(STATUS "Doxygen not found, not building docs")
# endif()


# set(CPACK_PROJECT_NAME ${PROJECT_NAME})
# set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
# include(CPack)
