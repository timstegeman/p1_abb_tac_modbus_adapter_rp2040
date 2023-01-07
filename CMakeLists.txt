cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
add_link_options("-Wl,--print-memory-usage")

project(p1_abb_tac_modbus_adapter_rp2040)

pico_sdk_init()

add_subdirectory(lib)

add_executable(p1_modbus
        src/main.c
        src/usb_descriptors.c
        src/config.c
        )

target_include_directories(p1_modbus PUBLIC inc)
target_link_libraries(p1_modbus PUBLIC modbus dsmr loadbalancer pico_stdlib tinyusb_device tinyusb_board)
pico_enable_stdio_usb(p1_modbus 1)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(p1_modbus)
