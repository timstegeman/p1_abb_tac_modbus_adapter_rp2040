add_library(modbus
        modbus.c
        modbus_client.c
        modbus_server.c
        )

target_include_directories(modbus PUBLIC ${CMAKE_CURRENT_LIST_DIR})
