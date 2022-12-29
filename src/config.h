//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#pragma once

#include <hardware/flash.h>
#include <hardware/sync.h>
#include <stdint.h>

#include "loadbalancer.h"

#define FLASH_SIZE          0x200000
#define FLASH_CONFIG_OFFSET (FLASH_SIZE - FLASH_PAGE_SIZE)

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
