//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include "modbus_common.h"

uint16_t mb_calc_crc16(const uint8_t* buf, uint8_t len) {
  uint16_t crc = 0xFFFF;
  uint8_t i, j;
  for (j = 0; j < len; j++) {
    crc ^= buf[j];
    for (i = 0; i < 8; i++) {
      if (crc & 0x01) {
        crc >>= 1;
        crc ^= 0xA001;
      } else
        crc >>= 1;
    }
  }
  return crc;
}
