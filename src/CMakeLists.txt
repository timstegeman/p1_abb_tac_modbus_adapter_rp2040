add_subdirectory(modbus)
add_subdirectory(dsmr)
add_subdirectory(loadbalancer)

add_executable(p1_modbus
        main.c
        usb_descriptors.c
        config.c
        )

target_include_directories(p1_modbus PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(p1_modbus PUBLIC modbus dsmr loadbalancer pico_stdlib tinyusb_device tinyusb_board)
pico_enable_stdio_usb(p1_modbus 1)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(p1_modbus)
