cmake_minimum_required(VERSION 3.20.0)
project(hello VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set (TEST_SRT OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

message("-- DEV CMAKE_PROJECT_NAME = ${CMAKE_PROJECT_NAME}")
message("-- DEV PROJECT_NAME = ${PROJECT_NAME}")
message("-- DEV MODULE_PATH = ${CMAKE_MODULE_PATH}")

# The compiled library code is here
# add_subdirectory(src)

# The executable code is here
# add_subdirectory(apps)



# include(CTest)
# enable_testing()

# include(ExternalProject)
# include(FetchContent)


add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(apps)
add_subdirectory(tests)







# find_package(Doxygen)
#   if(Doxygen_FOUND)
#     add_subdirectory(docs)
#   else()
#     message(STATUS "Doxygen not found, not building docs")
# endif()


# set(CPACK_PROJECT_NAME ${PROJECT_NAME})
# set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
# include(CPack)
