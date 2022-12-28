//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include "modbus_client.h"

#include <stdbool.h>
#include <string.h>

#include "modbus_internal.h"

#define MB_UINT16(data) (uint16_t) __builtin_bswap16(*(uint16_t*)&data)

enum mb_client_state {
  MB_DATA_READY = 0,  //
  MB_DATA_INCOMPLETE,
  MB_INVALID_SERVER_ADDRESS,
  MB_INVALID_FUNCTION
};

int mb_client_init(struct mb_client_context* ctx, struct mb_client_cb* cb) {
  memset(ctx, 0, sizeof(struct mb_client_context));
  ctx->cb = *cb;
  ctx->request_frame = (struct mb_rtu_frame*)ctx->request_buf;
  ctx->response_frame = (struct mb_rtu_frame*)ctx->response_buf;

  if (ctx->cb.tx == NULL || ctx->cb.get_tick_ms == NULL) {
    return -1;
  }

  return 0;
}

static inline void mb_reset_buf(struct mb_client_context* ctx) {
  ctx->response_buf_pos = 0;
}

void mb_client_rx(struct mb_client_context* ctx, uint8_t b) {
  if (ctx->cb.get_tick_ms() - ctx->timeout > MB_CLIENT_RECEIVE_TIMEOUT) {
    mb_reset_buf(ctx);
  }
  ctx->timeout = ctx->cb.get_tick_ms();
  if (ctx->response_buf_pos < (sizeof(ctx->response_buf) - 1)) {
    ctx->response_buf[ctx->response_buf_pos++] = b;
  }
}

static enum mb_client_state mb_check_buf(struct mb_client_context* ctx) {
  if (ctx->response_buf_pos > 4) {
    if (ctx->response_frame->address != ctx->request_frame->address) {
      return MB_INVALID_SERVER_ADDRESS;
    }
    switch (ctx->response_frame->function) {
      case MB_READ_COIL_STATUS:
      case MB_READ_INPUT_STATUS:
      case MB_READ_HOLDING_REGISTERS:
      case MB_READ_INPUT_REGISTERS:
        if (ctx->response_buf_pos == ctx->response_buf[2] + 4) {
          return MB_DATA_READY;
        }
        break;
      case MB_WRITE_SINGLE_COIL:
      case MB_WRITE_SINGLE_REGISTER:
      case MB_WRITE_MULTIPLE_COILS:
      case MB_WRITE_MULTIPLE_REGISTERS:
        if (ctx->request_buf_pos == 8) {
          return MB_DATA_READY;
        }
        break;
      default:
        return MB_INVALID_FUNCTION;
    }
  }
  return MB_DATA_INCOMPLETE;
}

static void swap16array(uint16_t* data, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    data[i] = __builtin_bswap16(data[i]);
  }
}

static void mb_rx_rtu(struct mb_client_context* ctx) {
  if (!ctx->busy) {
    // We didn't send a request
    if (ctx->cb.pass_thru) {
      ctx->cb.pass_thru(ctx->response_buf, ctx->response_buf_pos);
    }
    return;
  }

  if (mb_calc_crc16(ctx->response_buf, ctx->response_buf_pos)) {
    // Invalid CRC
    mb_reset_buf(ctx);
    return;
  }
  uint8_t address = ctx->response_frame->address;

  if (ctx->response_frame->function & 0x80) {
    if (ctx->cb.error) {
      ctx->cb.error(address, ctx->response_frame->function & ~0x80, ctx->response_frame->data[0]);
    }
    ctx->busy = false;
    mb_reset_buf(ctx);
    return;
  }

  switch (ctx->response_frame->function) {
    case MB_READ_COIL_STATUS:
      if (ctx->cb.read_coil_status) {
        ctx->cb.read_coil_status(address, ctx->request_start, ctx->request_count, &ctx->response_frame->data[1]);
      }
      break;
    case MB_READ_INPUT_STATUS:
      if (ctx->cb.read_coil_status) {
        ctx->cb.read_input_status(address, ctx->request_start, ctx->request_count, &ctx->response_frame->data[1]);
      }
      break;
    case MB_READ_HOLDING_REGISTERS:
      if (ctx->cb.read_holding_registers) {
        swap16array((uint16_t*)&ctx->response_frame->data[1], ctx->response_frame->data[0] / sizeof(uint16_t));
        ctx->cb.read_holding_registers(address, ctx->request_start, ctx->request_count,
                                       (uint16_t*)&ctx->response_frame->data[1]);
      }
      break;
    case MB_READ_INPUT_REGISTERS:
      if (ctx->cb.read_input_registers) {
        swap16array((uint16_t*)&ctx->response_frame->data[1], ctx->response_frame->data[0] / sizeof(uint16_t));
        ctx->cb.read_input_registers(address, ctx->request_start, ctx->request_count,
                                     (uint16_t*)&ctx->response_frame->data[1]);
      }
      break;
    case MB_WRITE_SINGLE_COIL:
      if (ctx->cb.write_single_coil) {
        ctx->cb.write_single_coil(address, MB_UINT16(ctx->response_frame->data[0]),
                                  MB_UINT16(ctx->response_frame->data[2]));
      }
      break;
    case MB_WRITE_SINGLE_REGISTER:
      if (ctx->cb.write_single_register) {
        ctx->cb.write_single_register(address, MB_UINT16(ctx->response_frame->data[0]),
                                      MB_UINT16(ctx->response_frame->data[2]));
      }
      break;
    case MB_WRITE_MULTIPLE_COILS:
      if (ctx->cb.write_multiple_coils) {
        ctx->cb.write_multiple_coils(address, MB_UINT16(ctx->response_frame->data[0]),
                                     MB_UINT16(ctx->response_frame->data[2]));
      }
      break;
    case MB_WRITE_MULTIPLE_REGISTERS:
      if (ctx->cb.write_multiple_registers) {
        ctx->cb.write_multiple_registers(address, MB_UINT16(ctx->response_frame->data[0]),
                                         MB_UINT16(ctx->response_frame->data[2]));
      }
      break;
    default:
      break;
  }

  ctx->busy = false;
  mb_reset_buf(ctx);
}

void mb_client_task(struct mb_client_context* ctx) {
  switch (mb_check_buf(ctx)) {
    case MB_INVALID_FUNCTION:
      mb_reset_buf(ctx);
      break;
    case MB_DATA_READY:
      mb_rx_rtu(ctx);
    default:
    case MB_DATA_INCOMPLETE:
      break;
  }
  if (ctx->request_timeout > 0 && ctx->cb.get_tick_ms() - ctx->request_timeout > MB_CLIENT_REQUEST_TIMEOUT) {
    ctx->busy = false;
    mb_reset_buf(ctx);
  }
}

static void mb_request_add(struct mb_client_context* ctx, uint16_t value) {
  ctx->request_buf[ctx->request_buf_pos++] = (value >> 8) & 0xFF;
  ctx->request_buf[ctx->request_buf_pos++] = value & 0xFF;
}

int mb_client_read_write(struct mb_client_context* ctx, uint8_t address, uint8_t fn, uint16_t start, uint16_t count) {
  if (ctx == NULL || address == 0) {
    return -1;
  }
  ctx->request_frame->address = address;
  ctx->request_frame->function = fn;
  ctx->request_buf_pos = sizeof(struct mb_rtu_frame);
  ctx->request_start = start;
  ctx->request_count = count;
  mb_request_add(ctx, start);
  mb_request_add(ctx, count);
  mb_request_add(ctx, __builtin_bswap16(mb_calc_crc16(ctx->request_buf, ctx->request_buf_pos)));
  ctx->cb.tx(ctx->request_buf, ctx->request_buf_pos);
  ctx->busy = true;
  ctx->request_timeout = ctx->cb.get_tick_ms();
  return 0;
}

int mb_client_read_coil_status(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t count) {
  return mb_client_read_write(ctx, address, MB_READ_COIL_STATUS, start, count);
}

int mb_client_read_input_status(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t count) {
  return mb_client_read_write(ctx, address, MB_READ_INPUT_STATUS, start, count);
}

int mb_client_read_holding_registers(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t count) {
  return mb_client_read_write(ctx, address, MB_READ_HOLDING_REGISTERS, start, count);
}

int mb_client_read_input_registers(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t count) {
  return mb_client_read_write(ctx, address, MB_READ_INPUT_REGISTERS, start, count);
}

int mb_client_write_single_coil(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t value) {
  return mb_client_read_write(ctx, address, MB_WRITE_SINGLE_COIL, start, value);
}

int mb_client_write_single_register(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t value) {
  return mb_client_read_write(ctx, address, MB_WRITE_SINGLE_REGISTER, start, value);
}

int mb_client_write_multiple_coils(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint8_t* data,
                                   uint16_t count) {
  // TODO
  return -1;
}

int mb_client_write_multiple_registers(struct mb_client_context* ctx, uint8_t address, uint16_t start, uint16_t* data,
                                       uint16_t count) {
  // TODO
  return -1;
}

bool mb_client_busy(struct mb_client_context* ctx) {
  return ctx->busy;
}