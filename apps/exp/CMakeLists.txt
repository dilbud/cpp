set(Boost_DEBUG OFF)
set(GST_DEBUG "*:WARN")

add_executable(main_exp 
    main_exp.cpp
    streamer.cpp

    # ${CMAKE_SOURCE_DIR}/extern/ffmpeg/fftools/cmdutils.c
    # ${CMAKE_SOURCE_DIR}/extern/ffmpeg/fftools/ffmpeg_filter.c
    # ${CMAKE_SOURCE_DIR}/extern/ffmpeg/fftools/ffmpeg_hw.c
    # ${CMAKE_SOURCE_DIR}/extern/ffmpeg/fftools/ffmpeg_opt.c
    # ${CMAKE_SOURCE_DIR}/extern/ffmpeg/fftools/ffmpeg.c 
    )

find_package(Boost REQUIRED 
            COMPONENTS
                thread
                )
find_package(gst REQUIRED)
# find_package(ffmpeg REQUIRED)
if(
    Boost_FOUND 
    AND 
    gst_FOUND
    # AND
    # ffmpeg_FOUND
    )
    target_link_libraries( main_exp 
        ${Boost_LIBRARIES} 
        ${gst_LIBRARIES} 
        # ${ffmpeg_LIBRARIES} 
    )
    # message(STATUS "${gst_INCLUDE_DIRS}")
    target_include_directories(main_exp 
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        PUBLIC ${Boost_INCLUDE_DIRS} 
        PUBLIC ${gst_INCLUDE_DIRS}
        # PUBLIC ${ffmpeg_INCLUDE_DIRS} 
        # PUBLIC ${CMAKE_SOURCE_DIR}/extern/ffmpeg
        # PUBLIC ${CMAKE_SOURCE_DIR}/extern/ffmpeg/fftools
        )
else()
    message(STATUS "Lib NOT FOUND")
endif()