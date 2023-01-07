//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>
#include <stdlib.h>

#define DSMR_RX_BUF_SIZE 128
enum dsmr_msg {
  MSG_ACTIVE_IMPORT_1,
  MSG_ACTIVE_IMPORT_2,
  MSG_VOLTAGE_L1,
  MSG_VOLTAGE_L2,
  MSG_VOLTAGE_L3,
  MSG_CURRENT_L1,
  MSG_CURRENT_L2,
  MSG_CURRENT_L3,
  MSG_POWER_L1,
  MSG_POWER_L2,
  MSG_POWER_L3,
  MSG_LAST
};

typedef void (*dsmr_value_cb_t)(enum dsmr_msg obj, float value);
typedef void (*dsmr_forward_cb_t)(char* data, size_t len);

void dsmr_init(dsmr_value_cb_t value_cb, dsmr_forward_cb_t forward_cb);
void dsmr_rx(char b);
void dsmr_task(void);
