//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#define MB_NO_ERROR                   0x00
#define MB_ERROR_ILLEGAL_FUNCTION     0x01
#define MB_ERROR_ILLEGAL_DATA_ADDRESS 0x02
#define MB_ERROR_ILLEGAL_DATA_VALUE   0x03
#define MB_ERROR_SLAVE_DEVICE_FAILURE 0x04

void mb_init(uint8_t slave_address);
void mb_rx(uint8_t b);
void mb_response_add(uint16_t value);
void mb_response_reset(uint8_t fn);
void mb_process();

extern uint32_t mb_get_tick_ms(void);
extern void mb_tx(uint8_t* data, uint32_t len);

extern uint8_t mb_read_coil_status(uint16_t start, uint16_t count);
extern uint8_t mb_read_input_status(uint16_t start, uint16_t count);
extern uint8_t mb_read_holding_registers(uint16_t start, uint16_t count);
extern uint8_t mb_read_input_registers(uint16_t start, uint16_t count);
extern uint8_t mb_write_single_coil(uint16_t start, uint16_t value);
extern uint8_t mb_write_single_register(uint16_t start, uint16_t value);
extern uint8_t mb_write_multiple_coils(uint16_t start, uint8_t* values, uint16_t len);
extern uint8_t mb_write_multiple_registers(uint16_t start, uint16_t* values, uint16_t len);
