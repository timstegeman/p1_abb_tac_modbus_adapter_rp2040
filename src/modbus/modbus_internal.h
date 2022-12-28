//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct __attribute((packed)) mb_rtu_frame {
  uint8_t address;
  uint8_t function;
  uint8_t data[];
};

uint16_t mb_calc_crc16(const uint8_t* buf, uint8_t len);
