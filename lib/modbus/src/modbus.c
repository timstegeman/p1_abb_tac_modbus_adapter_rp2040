//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include "modbus_common.h"

#define MODBUS_POLY 0xA001
#define MODBUS_SEED 0xFFFF

uint16_t mb_calc_crc16(const uint8_t* src, uint8_t len) {
  uint16_t crc = MODBUS_SEED;
  size_t i, j;

  for (i = 0; i < len; i++) {
    crc ^= (uint16_t)src[i];

    for (j = 0; j < 8; j++) {
      if (crc & 0x0001UL) {
        crc = (crc >> 1U) ^ MODBUS_POLY;
      } else {
        crc = crc >> 1U;
      }
    }
  }
  return __builtin_bswap16(crc);
}