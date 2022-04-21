add_subdirectory(modbus)
add_subdirectory(dsmr)

add_executable(p1_modbus
        main.c
        )

target_link_libraries(p1_modbus modbus dsmr pico_stdlib)
pico_enable_stdio_usb(p1_modbus 1)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(p1_modbus)
