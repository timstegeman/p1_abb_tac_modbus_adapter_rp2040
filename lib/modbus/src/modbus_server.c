//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include "modbus_server.h"

#include <string.h>

static enum mb_state mb_check_buf(struct mb_server_context* ctx) {
  if (ctx->request.pos > 4) {
    if (ctx->request.frame.function & 0x80) {
      if (ctx->request.pos == 5) {
        return MB_ERROR;
      }
    }
    switch (ctx->request.frame.function) {
      case MB_READ_COIL_STATUS:
      case MB_READ_INPUT_STATUS:
      case MB_READ_HOLDING_REGISTERS:
      case MB_READ_INPUT_REGISTERS:
      case MB_WRITE_SINGLE_COIL:
      case MB_WRITE_SINGLE_REGISTER:
        if (ctx->request.pos == 8) {
          return MB_DATA_READY;
        }
        break;
      case MB_WRITE_MULTIPLE_COILS:
      case MB_WRITE_MULTIPLE_REGISTERS:
        if (ctx->request.pos == ctx->request.data[6] + 9) {
          return MB_DATA_READY;
        }
        break;
      default:
        return MB_INVALID_FUNCTION;
    }
  }
  return MB_DATA_INCOMPLETE;
}

static inline void mb_reset(struct mb_server_context* ctx) {
  ctx->request.pos = 0;
}

static void mb_response_tx(struct mb_server_context* ctx) {
  // Calculate CRC
  uint16_t crc = mb_calc_crc16(ctx->response.data, ctx->response.pos);
  ctx->response.data[ctx->response.pos++] = (crc >> 8) & 0xFF;
  ctx->response.data[ctx->response.pos++] = crc & 0xFF;

  // Send RTU packet
  ctx->cb.tx(ctx->response.data, ctx->response.pos);
}

static void mb_error(struct mb_server_context* ctx, uint8_t err) {
  ctx->response.frame.address = ctx->address;
  ctx->response.frame.function = ctx->request.frame.function | 0x80;
  ctx->response.frame.data[0] = err;
  ctx->response.pos = offsetof(struct mb_server_buffer, frame.data) + 1;
  mb_response_tx(ctx);
}

void mb_server_add_response(struct mb_server_context* ctx, uint16_t value) {
  ctx->response.data[ctx->response.pos++] = (value >> 8) & 0xFF;
  ctx->response.data[ctx->response.pos++] = value & 0xFF;
  if (ctx->response.frame.function <= MB_READ_INPUT_REGISTERS) {
    ctx->response.frame.data[0] += sizeof(uint16_t);  // Size byte
  }
}

static void mb_rx_rtu(struct mb_server_context* ctx) {
  uint16_t registers[MB_MAX_REGISTERS];
  uint8_t res;

  // Check CRC
  if (mb_calc_crc16(ctx->request.data, ctx->request.pos)) {
    // Invalid CRC
    return;
  }

  if (ctx->request.frame.address != ctx->address || ctx->address == 0) {
    // It's a valid frame, but not for us. Maybe someone else can handle it
    if (ctx->cb.raw_rx) {
      ctx->cb.raw_rx(ctx->request.data, ctx->request.pos);
    }
    return;
  }

  uint16_t start = (uint16_t)__builtin_bswap16(*(uint16_t*)&ctx->request.frame.data[0]);
  uint16_t value = (uint16_t)__builtin_bswap16(*(uint16_t*)&ctx->request.frame.data[2]);

  ctx->response.frame.address = ctx->address;
  ctx->response.frame.function = ctx->request.frame.function;
  ctx->response.pos = offsetof(struct mb_server_buffer, frame.data);

  if (ctx->response.frame.function <= MB_READ_INPUT_REGISTERS) {
    ctx->response.frame.data[0] = 0;
    ctx->response.pos++;
  }

  res = MB_ERROR_ILLEGAL_DATA_ADDRESS;

  switch (ctx->request.frame.function) {
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
        res = ctx->cb.write_multiple_coils(start, &ctx->request.frame.data[5], value);
      }
      break;
    case MB_WRITE_MULTIPLE_REGISTERS:
      if (ctx->cb.write_multiple_registers) {
        // This will make sure the registers are aligned at 16 bits
        memcpy(registers, &ctx->request.frame.data[5], ctx->request.frame.data[4]);

        for (int i = 0; i < value; i++) {
          registers[i] = __builtin_bswap16(registers[i]);
        }
        res = ctx->cb.write_multiple_registers(start, registers, value);
      }
      break;
    default:
      break;
  }

  if (MB_NO_ERROR == res) {
    if (ctx->request.frame.function >= MB_WRITE_SINGLE_COIL) {
      mb_server_add_response(ctx, start);
      mb_server_add_response(ctx, value);
    }
    mb_response_tx(ctx);
  } else {
    mb_error(ctx, res);
  }
}

int mb_server_init(struct mb_server_context* ctx, uint8_t address, struct mb_server_cb* cb) {
  memset(ctx, 0, sizeof(struct mb_server_context));
  ctx->address = address;
  ctx->cb = *cb;

  if (ctx->cb.tx == NULL || ctx->cb.get_tick_ms == NULL) {
    return -1;
  }

  return 0;
}

void mb_server_rx(struct mb_server_context* ctx, uint8_t b) {
  if (ctx->cb.get_tick_ms() - ctx->timeout > MB_SERVER_RECEIVE_TIMEOUT) {
    mb_reset(ctx);
  }
  ctx->timeout = ctx->cb.get_tick_ms();
  if (ctx->request.pos < (sizeof(ctx->request.data) - 1)) {
    ctx->request.data[ctx->request.pos++] = b;
  }
}

void mb_server_task(struct mb_server_context* ctx) {
  switch (mb_check_buf(ctx)) {
    case MB_INVALID_FUNCTION:
      mb_error(ctx, MB_ERROR_ILLEGAL_FUNCTION);
      mb_reset(ctx);
    case MB_INVALID_SERVER_ADDRESS:
      mb_reset(ctx);
      break;
    case MB_ERROR:
    case MB_DATA_READY:
      mb_rx_rtu(ctx);
      mb_reset(ctx);
    default:
    case MB_DATA_INCOMPLETE:
      break;
  }
}
