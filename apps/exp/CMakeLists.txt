set(Boost_DEBUG OFF)
set(GST_DEBUG "*:WARN")

add_executable(main_exp main_exp.cpp streamer.cpp)

find_package(Boost REQUIRED 
            COMPONENTS
                thread)
find_package(gst REQUIRED)
find_package(ffmpeg REQUIRED)
if(
    Boost_FOUND 
    AND 
    gst_FOUND
    AND
    ffmpeg_FOUND
    )
    target_link_libraries( main_exp 
        ${Boost_LIBRARIES} 
        ${gst_LIBRARIES} 
        ffmpeg
    )
    message(STATUS "${gst_INCLUDE_DIRS}")
    target_include_directories(main_exp 
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        PUBLIC ${Boost_INCLUDE_DIRS} 
        PUBLIC ${gst_INCLUDE_DIRS} )
else()
    message(STATUS "Lib NOT FOUND")
endif()