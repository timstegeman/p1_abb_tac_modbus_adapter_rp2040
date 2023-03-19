//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

#include <hardware/flash.h>
#include <hardware/sync.h>
#include <stdint.h>

#include "loadbalancer.h"

#define FLASH_ROM_OFFSET    0x10000000
#define FLASH_CONFIG_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_PAGE_SIZE)

struct config {
  uint8_t address;
  struct lb_config lb_config;
  uint16_t _crc;
};
_Static_assert(sizeof(struct config) <= FLASH_PAGE_SIZE, "config struct too big");

extern struct config config;

void config_load(void);
void config_reset(void);
void config_save(void);
