//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

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

  void (*raw_rx)(uint8_t* data, size_t len);

  void (*tx)(uint8_t* data, size_t len);
  uint32_t (*get_tick_ms)(void);
};

struct mb_server_buffer {
  union {
    uint8_t data[MB_MAX_RTU_FRAME_SIZE];
    struct mb_rtu_frame frame;
  };
  size_t pos;
};

struct mb_server_context {
  uint8_t address;
  struct mb_server_cb cb;
  struct mb_server_buffer request;
  struct mb_server_buffer response;
  uint32_t timeout;
};

int mb_server_init(struct mb_server_context* ctx, uint8_t address, struct mb_server_cb* cb);
void mb_server_rx(struct mb_server_context* ctx, uint8_t b);
void mb_server_add_response(struct mb_server_context* ctx, uint16_t value);
void mb_server_task(struct mb_server_context* ctx);
