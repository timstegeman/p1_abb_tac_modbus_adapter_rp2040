add_library(loadbalancer
        loadbalancer.c
        )

target_include_directories(loadbalancer PUBLIC ${CMAKE_CURRENT_LIST_DIR})
