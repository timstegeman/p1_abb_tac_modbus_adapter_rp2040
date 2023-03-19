add_library(modbus
        src/modbus.c
        src/modbus_client.c
        src/modbus_server.c
        )

target_include_directories(modbus PUBLIC inc)
