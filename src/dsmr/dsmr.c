//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include "dsmr.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char dsmr_rx_buf[DSMR_RX_BUF_SIZE];
char dsmr_rx_line_buf[DSMR_RX_BUF_SIZE];
int dsmr_rx_buf_pos = 0;

static dsmr_update_cb_t dsmr_update_cb = NULL;

const char* DSMR_OBJ[] = {
    // Mapped to dsmr_msg_t. Other objects will be ignored.
    "1-0:1.8.1",   // MSG_ACTIVE_IMPORT_1
    "1-0:1.8.2",   // MSG_ACTIVE_IMPORT_2
    "1-0:32.7.0",  // MSG_VOLTAGE_L1
    "1-0:52.7.0",  // MSG_VOLTAGE_L2
    "1-0:72.7.0",  // MSG_VOLTAGE_L3
    "1-0:31.7.0",  // MSG_CURRENT_L1
    "1-0:51.7.0",  // MSG_CURRENT_L2
    "1-0:71.7.0",  // MSG_CURRENT_L3
    "1-0:21.7.0",  // MSG_POWER_L1
    "1-0:41.7.0",  // MSG_POWER_L2
    "1-0:61.7.0"   // MSG_POWER_L3
};

void dsmr_task(void) {
  uint16_t line_len = strlen(dsmr_rx_line_buf);
  if (line_len > 0) {
    for (int i = 0; i < MSG_LAST; i++) {
      uint16_t obj_len = strlen(DSMR_OBJ[i]);
      if (strncmp(DSMR_OBJ[i], dsmr_rx_line_buf, line_len > obj_len ? obj_len : line_len) == 0) {
        char* v = strchr(dsmr_rx_line_buf, '(') + 1;
        if (v && dsmr_update_cb) {
          dsmr_update_cb(i, strtof(v, NULL));
        }
        break;
      }
    }
    // Clear line
    dsmr_rx_line_buf[0] = '\0';
  }
}

void dsmr_init(dsmr_update_cb_t dsmr_update_cb_) {
  dsmr_update_cb = dsmr_update_cb_;
  dsmr_rx_buf_pos = 0;
  dsmr_rx_line_buf[0] = '\0';
}

void dsmr_rx(char b) {
  if (b == '\n' || b == '\r') {
    memcpy(dsmr_rx_line_buf, dsmr_rx_buf, dsmr_rx_buf_pos);
    dsmr_rx_line_buf[dsmr_rx_buf_pos] = '\0';
    dsmr_rx_buf_pos = 0;
  } else if (dsmr_rx_buf_pos < (sizeof(dsmr_rx_buf) - 1)) {
    dsmr_rx_buf[dsmr_rx_buf_pos++] = b;
  }
}
