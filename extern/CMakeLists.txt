
include(ExternalProject)
ExternalProject_Add(project_srt
        GIT_REPOSITORY https://github.com/Haivision/srt.git
        GIT_TAG v1.5.2
        GIT_SHALLOW 1
        GIT_PROGRESS 1
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/extern/lib/srt"
        BINARY_DIR "${CMAKE_SOURCE_DIR}/extern/lib/srt"
        CMAKE_ARGS -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        STEP_TARGETS build
        # EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )