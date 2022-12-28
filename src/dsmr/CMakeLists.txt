add_library(dsmr
        dsmr.c
        )

target_include_directories(dsmr PUBLIC ${CMAKE_CURRENT_LIST_DIR})
