add_library(loadbalancer
        src/loadbalancer.c
        )

target_include_directories(loadbalancer PUBLIC inc)
