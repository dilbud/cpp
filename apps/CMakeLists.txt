
if (TEST_SRT)
    add_subdirectory(srt)
elseif(TEST_OWT)
    add_subdirectory(owt)
elseif(TEST_UV)
    add_subdirectory(uv)
elseif(TEST_EXP)
    add_subdirectory(exp)
endif()