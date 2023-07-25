add_executable(main_exp main_exp.cpp)
set(Boost_DEBUG OFF)
find_package(Boost REQUIRED 
            COMPONENTS
                thread)
find_package(gst REQUIRED)
if(
    Boost_FOUND 
    AND 
    gst_FOUND
    )
    target_link_libraries( main_exp 
        ${Boost_LIBRARIES} 
        ${gst_LIBRARIES}
    )
    target_include_directories(main_exp PUBLIC ${Boost_INCLUDE_DIRS} PUBLIC ${gst_INCLUDE_DIRS})
    message(STATUS "${gst_INCLUDE_DIRS}")
endif()