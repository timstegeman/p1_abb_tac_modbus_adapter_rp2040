//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include "modbus_common.h"

#define MODBUS_POLY 0x8005
#define MODBUS_SEED 0xFFFF

uint16_t mb_calc_crc16(const uint8_t* buf, uint8_t len) {
  uint16_t crc = MODBUS_SEED;
  uint8_t i, j;
  for (i = 0; i < len; i++) {
    crc ^= buf[i];
    for (j = 0; j < 8; j++) {
      if (crc & 0x01) {
        crc >>= 1;
        crc ^= MODBUS_POLY;
      } else
        crc >>= 1;
    }
  }
  return crc;
}
