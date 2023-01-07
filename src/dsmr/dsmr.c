//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include "dsmr.h"

#include <stdbool.h>
#include <string.h>

#define DSMR_BUF_SIZE  512
#define DSMR_LINE_SIZE 256

static char dsmr_line[DSMR_LINE_SIZE];
static size_t dsmr_line_pos;
static char dsmr_buf[DSMR_BUF_SIZE];
static size_t dsmr_buf_head;
static size_t dsmr_buf_tail;
static bool dsmr_buf_full;

static dsmr_value_cb_t dsmr_value_cb = NULL;
static dsmr_forward_cb_t dsmr_forward_cb = NULL;

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

static int dsmr_parse_line(char* line, enum dsmr_msg* msg_type, float* value) {
  uint16_t line_len = strlen(line);
  for (int i = 0; i < MSG_LAST; i++) {
    uint16_t obj_len = strlen(DSMR_OBJ[i]);
    if (strncmp(DSMR_OBJ[i], line, line_len > obj_len ? obj_len : line_len) == 0) {
      char* v = strchr(line, '(') + 1;
      if (v && dsmr_value_cb) {
        *value = strtof(v, NULL);
        *msg_type = i;
        return 0;
      }
    }
  }
  return -1;
}

static int dsmr_buf_get(char* data) {
  if (!(!dsmr_buf_full && (dsmr_buf_head == dsmr_buf_tail))) {
    *data = dsmr_buf[dsmr_buf_tail];
    dsmr_buf_full = false;
    if (++(dsmr_buf_tail) == DSMR_BUF_SIZE) {
      dsmr_buf_tail = 0;
    }
    return 0;
  }
  return -1;
}

void dsmr_task(void) {
  enum dsmr_msg obj;
  float value;

  while (!dsmr_buf_get(&dsmr_line[dsmr_line_pos])) {
    if (dsmr_line[dsmr_line_pos] == '\n') {
      if (dsmr_forward_cb) {
        dsmr_forward_cb(dsmr_line, dsmr_line_pos + 1);
      }

      dsmr_line[dsmr_line_pos] = 0;
      if (!dsmr_parse_line(dsmr_line, &obj, &value)) {
        dsmr_value_cb(obj, value);
      }

      dsmr_line_pos = 0;
    } else {
      if (dsmr_line_pos++ >= DSMR_LINE_SIZE) {
        dsmr_line_pos = 0;
      }
    }
  }
}

void dsmr_init(dsmr_value_cb_t dsmr_value_cb_, dsmr_forward_cb_t dsmr_forward_cb_) {
  dsmr_value_cb = dsmr_value_cb_;
  dsmr_forward_cb = dsmr_forward_cb_;
  dsmr_buf_head = dsmr_buf_tail = 0;
  dsmr_line_pos = 0;
  dsmr_buf_full = false;
}

void dsmr_rx(char data) {
  dsmr_buf[dsmr_buf_head] = data;

  if (dsmr_buf_full) {
    if (++(dsmr_buf_tail) == DSMR_BUF_SIZE) {
      dsmr_buf_tail = 0;
    }
  }

  if (++(dsmr_buf_head) == DSMR_BUF_SIZE) {
    dsmr_buf_head = 0;
  }

  dsmr_buf_full = (dsmr_buf_head == dsmr_buf_tail);
}