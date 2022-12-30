//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modbus_common.h"

#define MB_CLIENT_RECEIVE_TIMEOUT 200
#define MB_CLIENT_REQUEST_TIMEOUT 500

struct mb_client_cb {
  void (*read_coil_status)(uint8_t address, uint16_t start, uint16_t count, uint8_t* data);
  void (*read_input_status)(uint8_t address, uint16_t start, uint16_t count, uint8_t* data);
  void (*read_holding_registers)(uint8_t address, uint16_t start, uint16_t count, uint16_t* data);
  void (*read_input_registers)(uint8_t address, uint16_t start, uint16_t count, uint16_t* data);
  void (*status)(uint8_t address, uint8_t function, uint8_t error_code);
  void (*pass_thru)(uint8_t* data, uint32_t len);

  void (*tx)(uint8_t* data, uint32_t len);
  uint32_t (*get_tick_ms)(void);
};

struct mb_client_context {
  struct mb_client_cb cb;
  uint8_t request_buf[MB_MAX_RTU_FRAME_SIZE];
  int request_buf_pos;
  uint8_t response_buf[MB_MAX_RTU_FRAME_SIZE];
  int response_buf_pos;
  uint32_t timeout;
  uint32_t request_timeout;
  uint16_t request_address;
  uint16_t request_start;
  uint16_t request_count;
  struct mb_rtu_frame* request_frame;
  struct mb_rtu_frame* response_frame;
  bool busy;
};

int mb_client_init(struct mb_client_context* ctx, struct mb_client_cb* cb);
int mb_client_read_coil_status(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t count);
int mb_client_read_input_status(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t count);
int mb_client_read_holding_registers(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t count);
int mb_client_read_input_registers(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t count);
int mb_client_write_single_coil(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t value);
int mb_client_write_single_register(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t value);
int mb_client_write_multiple_coils(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint8_t* data,
                                   uint16_t count);
int mb_client_write_multiple_registers(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t* data,
                                       uint16_t count);
void mb_client_rx(struct mb_client_context* ctx, uint8_t b);
void mb_client_task(struct mb_client_context* ctx);
bool mb_client_busy(struct mb_client_context* ctx);
