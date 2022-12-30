//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include "modbus_server.h"

#include <string.h>

#define MB_UINT16(data) (uint16_t) __builtin_bswap16(*(uint16_t*)&data)

enum mb_server_state {
  MB_DATA_READY = 0,  //
  MB_DATA_INCOMPLETE,
  MB_INVALID_SERVER_ADDRESS,
  MB_INVALID_FUNCTION
};

static enum mb_server_state mb_check_buf(struct mb_server_context* ctx) {
  if (ctx->request_buf_pos > 5) {
    switch (ctx->request_frame->function) {
      case MB_READ_COIL_STATUS:
      case MB_READ_INPUT_STATUS:
      case MB_READ_HOLDING_REGISTERS:
      case MB_READ_INPUT_REGISTERS:
      case MB_WRITE_SINGLE_COIL:
      case MB_WRITE_SINGLE_REGISTER:
        if (ctx->request_buf_pos == 8) {
          return MB_DATA_READY;
        }
        break;
      case MB_WRITE_MULTIPLE_COILS:
      case MB_WRITE_MULTIPLE_REGISTERS:
        if (ctx->request_buf_pos == ctx->request_buf[6] + 9) {
          return MB_DATA_READY;
        }
        break;
      default:
        return MB_INVALID_FUNCTION;
    }
  }
  return MB_DATA_INCOMPLETE;
}

static inline void mb_reset_buf(struct mb_server_context* ctx) {
  ctx->request_buf_pos = 0;
}

static void mb_response_tx(struct mb_server_context* ctx) {
  // Calculate CRC
  uint16_t crc = mb_calc_crc16(ctx->response_buf, ctx->response_buf_pos);
  ctx->response_buf[ctx->response_buf_pos++] = crc & 0xFF;
  ctx->response_buf[ctx->response_buf_pos++] = (crc >> 8) & 0xFF;

  // Send RTU packet
  ctx->cb.tx(ctx->response_buf, ctx->response_buf_pos);
}

static void mb_error(struct mb_server_context* ctx, uint8_t err) {
  ctx->response_frame->address = ctx->address;
  ctx->response_frame->function = ctx->request_frame->function | 0x80;
  ctx->response_frame->data[0] = err;
  ctx->response_buf_pos = sizeof(struct mb_rtu_frame) + 1;
  mb_response_tx(ctx);
}

void mb_server_response_add(struct mb_server_context* ctx, uint16_t value) {
  ctx->response_buf[ctx->response_buf_pos++] = (value >> 8) & 0xFF;
  ctx->response_buf[ctx->response_buf_pos++] = value & 0xFF;

  if (ctx->response_frame->function <= MB_READ_INPUT_REGISTERS) {
    ctx->response_frame->data[0] += sizeof(uint16_t);  // Size byte
  }
}

void mb_response_reset(struct mb_server_context* ctx, uint8_t fn) {
  ctx->response_frame->address = ctx->address;
  ctx->response_frame->function = fn;
  ctx->response_buf_pos = sizeof(struct mb_rtu_frame);

  if (ctx->response_frame->function <= MB_READ_INPUT_REGISTERS) {
    ctx->response_frame->data[0] = 0;
    ctx->response_buf_pos++;
  }
}

static void mb_rx_rtu(struct mb_server_context* ctx) {
  uint16_t registers[MB_MAX_REGISTERS];
  uint8_t res;

  if (ctx->request_frame->address != ctx->address || ctx->address == 0) {
    if (ctx->cb.pass_thru) {  // It's a valid frame, but not for use. Maybe someone else can handle it
      ctx->cb.pass_thru(ctx->request_buf, ctx->request_buf_pos);
    }
    return;
  }

  // Check CRC
  if (mb_calc_crc16(ctx->request_buf, ctx->request_buf_pos)) {
    // Invalid CRC
    mb_reset_buf(ctx);
    return;
  }

  uint16_t start = MB_UINT16(ctx->request_frame->data[0]);
  uint16_t value = MB_UINT16(ctx->request_frame->data[2]);
  //  uint8_t* data = &ctx->request_frame->data[5];

  mb_response_reset(ctx, ctx->request_frame->function);
  res = MB_ERROR_ILLEGAL_DATA_ADDRESS;

  switch (ctx->request_frame->function) {
    case MB_READ_COIL_STATUS:
      if (ctx->cb.read_coil_status) {
        res = ctx->cb.read_coil_status(start, value);
      }
      break;
    case MB_READ_INPUT_STATUS:
      if (ctx->cb.read_coil_status) {
        res = ctx->cb.read_input_status(start, value);
      }
      break;
    case MB_READ_HOLDING_REGISTERS:
      if (ctx->cb.read_holding_registers) {
        res = ctx->cb.read_holding_registers(start, value);
      }
      break;
    case MB_READ_INPUT_REGISTERS:
      if (ctx->cb.read_input_registers) {
        res = ctx->cb.read_input_registers(start, value);
      }
      break;
    case MB_WRITE_SINGLE_COIL:
      if (ctx->cb.write_single_coil) {
        res = ctx->cb.write_single_coil(start, value);
      }
      break;
    case MB_WRITE_SINGLE_REGISTER:
      if (ctx->cb.write_single_register) {
        res = ctx->cb.write_single_register(start, value);
      }
      break;
    case MB_WRITE_MULTIPLE_COILS:
      if (ctx->cb.write_multiple_coils) {
        res = ctx->cb.write_multiple_coils(start, &ctx->request_frame->data[5], value);
        if (res == MB_NO_ERROR) {
          mb_server_response_add(ctx, start);
          mb_server_response_add(ctx, value);
        }
      }
      break;
    case MB_WRITE_MULTIPLE_REGISTERS:
      if (ctx->cb.write_multiple_registers) {
        // This will make sure the registers are aligned at 16 bits
        memcpy(registers, &ctx->request_frame->data[5], ctx->request_frame->data[4]);

        for (int i = 0; i < value; i++) {
          registers[i] = __builtin_bswap16(registers[i]);
        }
        res = ctx->cb.write_multiple_registers(start, registers, value);
        if (res == MB_NO_ERROR) {
          mb_server_response_add(ctx, start);
          mb_server_response_add(ctx, value);
        }
      }
      break;
    default:
      res = MB_ERROR_ILLEGAL_FUNCTION;
      break;
  }

  if (res == MB_NO_ERROR) {
    mb_response_tx(ctx);
  } else {
    mb_error(ctx, res);
  }

  mb_reset_buf(ctx);
}

int mb_server_init(struct mb_server_context* ctx, uint8_t address, struct mb_server_cb* cb) {
  memset(ctx, 0, sizeof(struct mb_server_context));
  ctx->address = address;
  ctx->cb = *cb;
  ctx->request_frame = (struct mb_rtu_frame*)ctx->request_buf;
  ctx->response_frame = (struct mb_rtu_frame*)ctx->response_buf;

  if (ctx->cb.tx == NULL || ctx->cb.get_tick_ms == NULL) {
    return -1;
  }

  return 0;
}

void mb_server_rx(struct mb_server_context* ctx, uint8_t b) {
  if (ctx->cb.get_tick_ms() - ctx->timeout > MB_SERVER_RECEIVE_TIMEOUT) {
    mb_reset_buf(ctx);
  }
  ctx->timeout = ctx->cb.get_tick_ms();
  if (ctx->request_buf_pos < (sizeof(ctx->request_buf) - 1)) {
    ctx->request_buf[ctx->request_buf_pos++] = b;
  }
}

void mb_server_task(struct mb_server_context* ctx) {
  switch (mb_check_buf(ctx)) {
    case MB_INVALID_FUNCTION:
      mb_error(ctx, MB_ERROR_ILLEGAL_FUNCTION);
    case MB_INVALID_SERVER_ADDRESS:
      mb_reset_buf(ctx);
      break;
    case MB_DATA_READY:
      mb_rx_rtu(ctx);
    default:
    case MB_DATA_INCOMPLETE:
      break;
  }
}
