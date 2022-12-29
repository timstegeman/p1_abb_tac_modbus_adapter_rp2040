//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include "loadbalancer.h"

#include <string.h>

#define WAIT_TIME_UNSET   0xFF
#define CHECK_INTERVAL_MS 1000

static struct lb_config config;
static lb_limit_charger_cb_t lb_limit_charger_cb;
static uint16_t grid_current[3];
static int charger_max_current;
static enum lb_state state;
static uint8_t wait_time, grid_current_age;
static int charger_limit_override;

void lb_init(struct lb_config* config_, lb_limit_charger_cb_t lb_limit_charger_cb_) {
  config = *config_;
  lb_limit_charger_cb = lb_limit_charger_cb_;
  memset(grid_current, 0, sizeof(grid_current));
  state = LB_STATE_NORMAL;
  charger_max_current = 0;
  wait_time = WAIT_TIME_UNSET;
  grid_current_age = WAIT_TIME_UNSET;
}

void lb_set_grid_current(enum lb_phase phase, uint16_t current) {
  grid_current[phase] = current;
  grid_current_age = 0;
}

void lb_set_charger_limit_override(uint16_t limit) {
  charger_limit_override = limit;
}

uint16_t lb_get_charger_limit_override(void) {
  return charger_limit_override;
}

static uint16_t get_max_grid_current(void) {
  uint16_t max = 0;
  for (enum lb_phase phase = 0; phase < config.number_of_phases; phase++) {
    if (grid_current[phase] > max) {
      max = grid_current[phase];
    }
  }
  return max;
}

static void set_state(enum lb_state state_) {
  if (state != state_) {
    state = state_;
    wait_time = WAIT_TIME_UNSET;
  }
}

static void lb_check(uint32_t now) {
  static uint32_t last_update = 0;
  uint16_t grid_current_max = get_max_grid_current();
  uint32_t time_diff = 0;

  if (last_update) {
    time_diff = now - last_update;
  }
  last_update = now;

  if (wait_time != WAIT_TIME_UNSET) {
    if (wait_time > 0 && time_diff < wait_time) {
      wait_time -= time_diff;
    } else {
      wait_time = 0;
    }
  }

  if (grid_current_age < 0xFF && time_diff < (0xFF - grid_current_age)) {
    grid_current_age += time_diff;
  } else {
    grid_current_age = 0xFF;
  }

  if (grid_current_age >= config.fallback_limit_wait_time) {
    set_state(LB_STATE_FALLBACK);
  } else if (grid_current_max > config.alarm_limit) {
    set_state(LB_STATE_ALARM_LIMIT);
  } else if (grid_current_max > config.upper_limit) {
    set_state(LB_STATE_UPPER_LIMIT);
  } else if (grid_current_max <= config.lower_limit) {
    set_state(LB_STATE_LOWER_LIMIT);
  } else {
    set_state(LB_STATE_NORMAL);
  }

  switch (state) {
    case LB_STATE_LOWER_LIMIT:  // Below the Lower limit: when the energy meter provides meter data to show sufficient
                                // room to change configuration the charger will start increasing the power output (if
                                // the charger is below its maximal rated current) by a certain amount.

      if (wait_time == WAIT_TIME_UNSET) {
        wait_time = config.lower_limit_wait_time;
      } else if (wait_time == 0) {
        wait_time = WAIT_TIME_UNSET;
        charger_max_current += config.lower_limit_change_amount;
      }
      break;
    case LB_STATE_NORMAL:  // area between the Upper limit and Lower limit. This is considered a safe area for the
                           // charger to operate at its current power output. Within this region, there is no reason to
                           // change the output current of our  charger.
      break;
    case LB_STATE_UPPER_LIMIT:  // between the Upper limit and Alarm limit: the charger must also decrease power
                                // output in periods that the external meter provides information that there is limited
                                // current available  (since it might be used by other electrical appliances). In this
                                // region, the charger will decrease its power output by a certain amount.

      if (wait_time == WAIT_TIME_UNSET) {
        wait_time = config.upper_limit_wait_time;
      } else if (wait_time == 0) {
        wait_time = WAIT_TIME_UNSET;
        charger_max_current -= config.upper_limit_change_amount;
      }
      break;
    case LB_STATE_ALARM_LIMIT:  // between the alarm limit and grid limit (Electrical capacity), an immediate response
                                // is needed to avoid over current and to protect the fuse. If the external meter gives
                                // information to the charger that the current of a particular phase is in this area,
                                // the charger will react by decreasing the power output by a considerable amount.

      if (wait_time == WAIT_TIME_UNSET) {
        wait_time = config.alarm_limit_wait_time;
      } else if (wait_time == 0) {
        charger_max_current -= config.alarm_limit_change_amount;
        wait_time = WAIT_TIME_UNSET;
      }
      break;

    default:
    case LB_STATE_FALLBACK:  // grid current is not updated for some time. Set the charger to a current which will not
                             // cause and over current on the system
      if (wait_time == WAIT_TIME_UNSET) {
        wait_time = config.fallback_limit_wait_time;
      } else if (wait_time == 0) {
        wait_time = WAIT_TIME_UNSET;
        charger_max_current = config.fallback_limit;
      }
      break;
  }

  if (charger_max_current < 0) {  // there is probably an over current somewhere else in the system
    charger_max_current = 0;
  } else if (charger_max_current > config.charger_limit) {
    if (charger_max_current > charger_limit_override) {
      charger_max_current = charger_limit_override;
    } else {
      charger_max_current = config.charger_limit;
    }
  }

  if (lb_limit_charger_cb) {
    lb_limit_charger_cb(charger_max_current);
  }
}

void lb_task(uint32_t now) {
  static uint32_t last = 0;

  if ((now - last) > CHECK_INTERVAL_MS) {
    lb_check(now / 1000);
  }

  last = now;
}

uint16_t lb_get_limit(void) {
  return charger_max_current;
};

enum lb_state lb_get_state(void) {
  return state;
};

#if 0
//TODO Move to unit test
static void limit_charger(uint16_t current) {
  printf("Limit charger current to %d\n", current);
}


void test(void) {
  uint16_t grid_limit = 25000;
  struct lb_config my_config = {.charger_limit = 16000,
                                .number_of_phases = 1,
                                .alarm_limit = grid_limit - 1000,
                                .alarm_limit_wait_time = 1,
                                .alarm_limit_change_amount = grid_limit / 2,
                                .upper_limit = grid_limit - 3000,
                                .upper_limit_wait_time = 5,
                                .upper_limit_change_amount = 1000,
                                .lower_limit = grid_limit - 6000,
                                .lower_limit_change_amount = 1000,
                                .fallback_limit = 0,
                                .fallback_limit_wait_time = 30};

  lb_init(&my_config, &limit_charger);
  //  lb_set_charger_limit(16000);

  //  uint16_t grid = 23000;
  //  uint16_t charger_load = 16000
  //  lb_set_grid_current(LB_PHASE_1, 24500);
  for (int i = 0; i < 3600; i++) {
    //    printf("time: %d\n", i);
    if (i < 300) {  // 5 min
      lb_set_grid_current(LB_PHASE_1, 24500);
    } else if (i < 300 * 2) {  // 10 min
      lb_set_grid_current(LB_PHASE_1, 19000);
    } else if (i < 300 * 3) {  // 15 min
      lb_set_grid_current(LB_PHASE_1, 23000);
    } else if (i < 300 * 4) {  // 20 min
      lb_set_grid_current(LB_PHASE_1, 10000);
    } else {
      lb_set_grid_current(LB_PHASE_1, 19000);
    }
    lb_task(i);
  }
}
#endif