//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include "modbus_client.h"

// Support for ABB Terra AC charger

#define ABB_TAC_ADDRESS                    1
#define ABB_TAC_SET_CHARGING_CURRENT_LIMIT 0x4100

void limit_charger(struct mb_client_context* ctx, uint16_t current) {
  if (current > 15650) {  // Somehow the charger is reacting strange when we set it to a higher value. This value causes
                          // the charger to go to 16 amps anyway.
    current = 15650;
  }

  uint32_t value32 = current << 16;  // We only have 16 bits
  mb_client_write_multiple_registers(ctx, ABB_TAC_ADDRESS, ABB_TAC_SET_CHARGING_CURRENT_LIMIT, (uint16_t*)&value32, 2);
}
