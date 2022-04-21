//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

#define DSMR_RX_BUF_SIZE 64

typedef enum {
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
} dsmr_msg_t;

extern void dsmr_update(dsmr_msg_t obj, float value);

void dsmr_init(void);
void dsmr_rx(char b);
void dsmr_process(void);
