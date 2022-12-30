//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include "config.h"

#include <string.h>

#include "modbus_common.h"  // For CRC16

struct config config;

void config_load(void) {
  memcpy(&config, (void*)(FLASH_CONFIG_OFFSET + FLASH_ROM_OFFSET), sizeof(struct config));
  if (mb_calc_crc16((uint8_t*)&config, sizeof(struct config))) {
    config_reset();
  }
}

void config_reset(void) {

  // These are the (factory/my home) defaults
  config.address = 10;
  config.lb_config.charger_limit = 16000;
  config.lb_config.number_of_phases = 3;
  config.lb_config.alarm_limit = 24000;
  config.lb_config.alarm_limit_wait_time = 1;
  config.lb_config.alarm_limit_change_amount = 12500;
  config.lb_config.upper_limit = 22000;
  config.lb_config.upper_limit_wait_time = 5;
  config.lb_config.upper_limit_change_amount = 1000;
  config.lb_config.lower_limit = 19000;
  config.lb_config.lower_limit_change_amount = 1000;
  config.lb_config.fallback_limit = 0;
  config.lb_config.fallback_limit_wait_time = 30;

  config_save();
}

void config_save(void) {
  config._crc = mb_calc_crc16((uint8_t*)&config, offsetof(struct config, _crc));

  uint32_t interrupts = save_and_disable_interrupts();

  flash_range_erase(FLASH_CONFIG_OFFSET, FLASH_PAGE_SIZE);
  flash_range_program(FLASH_CONFIG_OFFSET, (uint8_t*)&config, sizeof(struct config));

  restore_interrupts(interrupts);
}
