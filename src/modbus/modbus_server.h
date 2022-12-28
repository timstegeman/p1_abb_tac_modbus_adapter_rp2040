//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modbus_common.h"

#define MB_SERVER_RECEIVE_TIMEOUT 100

struct mb_server_cb {
  enum mb_result (*read_coil_status)(uint16_t start, uint16_t count);
  enum mb_result (*read_input_status)(uint16_t start, uint16_t count);
  enum mb_result (*read_holding_registers)(uint16_t start, uint16_t count);
  enum mb_result (*read_input_registers)(uint16_t start, uint16_t count);
  enum mb_result (*write_single_coil)(uint16_t start, uint16_t value);
  enum mb_result (*write_single_register)(uint16_t start, uint16_t value);
  enum mb_result (*write_multiple_coils)(uint16_t start, uint8_t* data, uint16_t count);
  enum mb_result (*write_multiple_registers)(uint16_t start, uint16_t* data, uint16_t count);

  void (*pass_thru)(uint8_t* data, uint32_t len);

  void (*tx)(uint8_t* data, uint32_t len);
  uint32_t (*get_tick_ms)(void);
};

struct mb_server_context {
  uint8_t address;
  struct mb_server_cb cb;
  uint8_t request_buf[MB_MAX_RTU_FRAME_SIZE];
  int request_buf_pos;
  uint8_t response_buf[MB_MAX_RTU_FRAME_SIZE];
  int response_buf_pos;
  uint32_t timeout;
  struct mb_rtu_frame* request_frame;
  struct mb_rtu_frame* response_frame;
};

int mb_server_init(struct mb_server_context* ctx, uint8_t address, struct mb_server_cb* cb);
void mb_server_rx(struct mb_server_context* ctx, uint8_t b);
void mb_server_response_add(struct mb_server_context* ctx, uint16_t value);
void mb_server_task(struct mb_server_context* ctx);
