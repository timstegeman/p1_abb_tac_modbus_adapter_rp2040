cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
add_link_options("-Wl,--print-memory-usage")

project(p1_abb_tac_modbus_adapter_rp2040)

pico_sdk_init()

add_subdirectory(src)
