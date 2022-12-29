//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>
#define MB_MAX_RTU_FRAME_SIZE 256

enum mb_result {
  MB_NO_ERROR = 0x00,
  MB_ERROR_ILLEGAL_FUNCTION = 0x01,
  MB_ERROR_ILLEGAL_DATA_ADDRESS = 0x02,
  MB_ERROR_ILLEGAL_DATA_VALUE = 0x03,
  MB_ERROR_SLAVE_DEVICE_FAILURE = 0x04,
};

enum mb_function {
  MB_READ_COIL_STATUS = 0x01,
  MB_READ_INPUT_STATUS = 0x02,
  MB_READ_HOLDING_REGISTERS = 0x03,
  MB_READ_INPUT_REGISTERS = 0x04,
  MB_WRITE_SINGLE_COIL = 0x05,
  MB_WRITE_SINGLE_REGISTER = 0x06,
  MB_WRITE_MULTIPLE_COILS = 0x10,
  MB_WRITE_MULTIPLE_REGISTERS = 0x0F
};

struct __attribute((packed)) mb_rtu_frame {
  uint8_t address;
  uint8_t function;
  uint8_t data[];
};

uint16_t mb_calc_crc16(const uint8_t* buf, uint8_t len);
