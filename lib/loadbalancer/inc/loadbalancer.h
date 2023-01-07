//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

enum lb_phase {
  LB_PHASE_1 = 0,
  LB_PHASE_2,
  LB_PHASE_3,
};

enum lb_state {
  LB_STATE_NORMAL = 0,
  LB_STATE_LOWER_LIMIT,
  LB_STATE_UPPER_LIMIT,
  LB_STATE_ALARM_LIMIT,
  LB_STATE_FALLBACK,
};

struct lb_config {
  // current in mA, time in seconds
  uint16_t charger_limit;
  uint8_t number_of_phases;
  uint16_t alarm_limit;
  uint8_t alarm_limit_wait_time;
  uint16_t alarm_limit_change_amount;
  uint16_t upper_limit;
  uint8_t upper_limit_wait_time;
  uint16_t upper_limit_change_amount;
  uint16_t lower_limit;
  uint8_t lower_limit_wait_time;
  uint16_t lower_limit_change_amount;
  uint16_t fallback_limit;
  uint8_t fallback_limit_wait_time;
};

typedef void (*lb_limit_charger_cb_t)(uint16_t current);

void lb_init(struct lb_config* config, lb_limit_charger_cb_t cb);
void lb_set_grid_current(enum lb_phase phase, uint16_t current);
void lb_set_charger_limit_override(uint16_t limit);
uint16_t lb_get_charger_limit_override(void);
enum lb_state lb_get_state(void);
uint16_t lb_get_limit(void);
void lb_task(uint32_t current_time);
