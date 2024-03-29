//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

#include "modbus_common.h"

#define MB_CLIENT_REQUEST_TIMEOUT 1000
#define MB_CLIENT_QUEUE_SIZE      10

struct mb_client_cb {
  void (*read_coil_status)(uint8_t address, uint16_t start, uint16_t count, uint8_t* data);
  void (*read_input_status)(uint8_t address, uint16_t start, uint16_t count, uint8_t* data);
  void (*read_holding_registers)(uint8_t address, uint16_t start, uint16_t count, uint16_t* data);
  void (*read_input_registers)(uint8_t address, uint16_t start, uint16_t count, uint16_t* data);
  void (*status)(uint8_t address, uint8_t function, uint8_t error_code);
  void (*raw_rx)(uint8_t* data, size_t len);

  void (*tx)(uint8_t* data, size_t len);
  uint32_t (*get_tick_ms)(void);
};

struct mb_client_buffer {
  union {
    uint8_t data[MB_MAX_RTU_FRAME_SIZE];
    struct mb_rtu_frame frame;
  };
  size_t pos;
  uint16_t start;
  uint16_t count;
  bool raw;
  bool ready;
};

struct mb_client_context {
  struct mb_client_cb cb;
  struct mb_client_buffer* current_request;
  struct mb_client_buffer request_queue[MB_CLIENT_QUEUE_SIZE];
  struct mb_client_buffer response;
  uint32_t request_timeout;
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

int mb_client_send_raw(struct mb_client_context* ctx, uint8_t* data, size_t len);

void mb_client_rx(struct mb_client_context* ctx, uint8_t b);
void mb_client_task(struct mb_client_context* ctx);
