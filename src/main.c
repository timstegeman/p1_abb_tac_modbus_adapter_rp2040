//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include <hardware/irq.h>
#include <hardware/structs/scb.h>
#include <hardware/uart.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <pico/stdlib.h>
#include <stdio.h>

#include "bsp/board.h"
#include "config.h"
#include "dsmr.h"
#include "loadbalancer.h"
#include "modbus_client.h"
#include "modbus_server.h"
#include "registers.h"
#include "tusb.h"

#define DSMR_UART      uart0
#define DSMR_UART_IRQ  UART0_IRQ
#define DSMR_UART_BAUD 115200  // P1/Smartmeter
#define DSMR_RX_PIN    17
#define MB_UART        uart1
#define MB_UART_IRQ    UART1_IRQ
#define MB_UART_BAUD   9600  // RS485/Modbus
#define MB_DE_PIN      7
#define MB_TX_PIN      8
#define MB_RX_PIN      9
#define USB_ITF_DSMR   0
#define USB_ITF_MB     1

static struct mb_server_context mb_server_ctx;
static struct mb_client_context mb_client_ctx;
static uint16_t system_error;

static void mb_client_tx(uint8_t* data, size_t size) {
  gpio_put(MB_DE_PIN, 1);
  uart_write_blocking(MB_UART, data, size);

  // Wait until fifo is drained so we now when to turn off the driver enable pin.
  uart_tx_wait_blocking(MB_UART);
  gpio_put(MB_DE_PIN, 0);
}

static void on_mb_rx(void) {  // Interrupt
  while (uart_is_readable(MB_UART)) {
    mb_client_rx(&mb_client_ctx, uart_getc(MB_UART));
  }
}

static uint32_t mb_get_tick_ms(void) {
  return time_us_64() / 1000;
}

static void on_dsmr_rx(void) {  // Interrupt
  while (uart_is_readable(DSMR_UART)) {
    dsmr_rx(uart_getc(DSMR_UART));
  }
}

static void dsmr_forward(char* data, size_t size) {
  if (tud_cdc_n_connected(USB_ITF_DSMR)) {
    tud_cdc_n_write(USB_ITF_DSMR, data, size);
    tud_cdc_write_flush();
  }
}

static void mb_server_tx(uint8_t* data, size_t size) {
  if (tud_cdc_n_connected(USB_ITF_MB)) {
    tud_cdc_n_write(USB_ITF_MB, data, size);
    tud_cdc_n_write_flush(USB_ITF_MB);
  }
}

void tud_cdc_rx_cb(uint8_t i) {  // Interrupt
  char c;
  if (USB_ITF_MB == i) {
    while (tud_cdc_n_available(i)) {
      if (tud_cdc_n_read(i, &c, i)) {
        mb_server_rx(&mb_server_ctx, c);
      }
    }
  }
}

static void dsmr_update(enum dsmr_msg obj, float value) {
  switch (obj) {
    case MSG_CURRENT_L1:
      lb_set_grid_current(LB_PHASE_1, (uint32_t)value * 1000);
      break;
    case MSG_CURRENT_L2:
      lb_set_grid_current(LB_PHASE_2, (uint32_t)value * 1000);
      break;
    case MSG_CURRENT_L3:
      lb_set_grid_current(LB_PHASE_3, (uint32_t)value * 1000);
      break;
    default:
      break;
  }
}

static void mb_client_status(uint8_t address, uint8_t function, uint8_t error_) {
  (void)address;
  (void)function;
  system_error |= error_ & 0xFF;
}

static void limit_charger(uint16_t current) {
  // Only support for ABB Terra AC charger

  /* For quantities that are represented as more than 1 register, the most significant
     byte is found in the high byte of the first (lowest) register. The least significant
     byte is found in the low byte of the last (highest) register. */

  //  printf("# Limit charger to %d mA\n", current);

#define ABB_TAC_ADDRESS                    1
#define ABB_TAC_SET_CHARGING_CURRENT_LIMIT 0x4100

  uint32_t value32 = current;
  value32 = __builtin_bswap32(value32);

  mb_client_write_multiple_registers(&mb_client_ctx, ABB_TAC_ADDRESS, ABB_TAC_SET_CHARGING_CURRENT_LIMIT,
                                     (uint16_t*)&value32, 2);
}

static void sys_reset(void) {
  scb_hw->aircr |= M0PLUS_AIRCR_SYSRESETREQ_BITS;
  for (;;) {
  }
}

static enum mb_result write_single_holding_register(uint16_t reg, uint16_t value) {
  switch (reg) {
    case MB_REG_CHARGER_LIMIT_OVERRIDE:
      lb_set_charger_limit_override(value);
      return MB_NO_ERROR;
    case MB_REG_CONFIG_CHARGER_LIMIT:
      config.lb_config.charger_limit = value;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_NUMBER_OF_PHASES:
      if (value < 1 || value > 3) {
        return MB_ERROR_ILLEGAL_DATA_VALUE;
      }
      config.lb_config.number_of_phases = value & 0xF;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_ALARM_LIMIT:
      config.lb_config.alarm_limit = value;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_ALARM_LIMIT_WAIT_TIME:
      if (value >= 0xFF) {
        return MB_ERROR_ILLEGAL_DATA_VALUE;
      }
      config.lb_config.alarm_limit_wait_time = value & 0xFF;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_ALARM_LIMIT_CHANGE_AMOUNT:
      config.lb_config.alarm_limit_change_amount = value;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_UPPER_LIMIT:
      config.lb_config.upper_limit = value;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_UPPER_LIMIT_WAIT_TIME:
      if (value >= 0xFF) {
        return MB_ERROR_ILLEGAL_DATA_VALUE;
      }
      config.lb_config.upper_limit_wait_time = value & 0xFF;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_UPPER_LIMIT_CHANGE_AMOUNT:
      config.lb_config.upper_limit_change_amount = value;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_LOWER_LIMIT:
      config.lb_config.lower_limit = value;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_LOWER_LIMIT_WAIT_TIME:
      if (value >= 0xFF) {
        return MB_ERROR_ILLEGAL_DATA_VALUE;
      }
      config.lb_config.lower_limit_wait_time = value & 0xFF;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_LOWER_LIMIT_CHANGE_AMOUNT:
      config.lb_config.lower_limit_change_amount = value;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_FALLBACK_LIMIT:
      config.lb_config.fallback_limit = value;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_FALLBACK_LIMIT_WAIT_TIME:
      if (value >= 0xFF) {
        return MB_ERROR_ILLEGAL_DATA_VALUE;
      }
      config.lb_config.fallback_limit_wait_time = value & 0xFF;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_ADDRESS:
      if (value >= 0xFF) {
        return MB_ERROR_ILLEGAL_DATA_VALUE;
      }
      config.address = value & 0xFF;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_APPLY:
      if (value == 1) {
        config_save();
        sys_reset();
      }
      return MB_ERROR_ILLEGAL_DATA_VALUE;
    case MB_REG_CONFIG_FACTORY_RESET:
      if (value == 1) {
        config_reset();
        sys_reset();
        return MB_NO_ERROR;
      }
      return MB_ERROR_ILLEGAL_DATA_VALUE;
    default:
      return MB_ERROR_ILLEGAL_DATA_ADDRESS;
  }
}

static enum mb_result write_holding_registers(uint16_t start, uint16_t* data, uint16_t count) {
  enum mb_result res;

  for (int i = 0; i < count; i++) {
    res = write_single_holding_register(start + i, data[i]);
    if (res != MB_NO_ERROR) {
      return res;
    }
  }
  return MB_NO_ERROR;
}

static enum mb_result read_single_holding_register(uint16_t reg, uint16_t* value) {
  switch (reg) {
    case MB_REG_CHARGER_LIMIT_OVERRIDE:
      *value = lb_get_charger_limit_override();
      return MB_NO_ERROR;
    case MB_REG_CURRENT_LIMIT:
      *value = lb_get_limit();
      return MB_NO_ERROR;
    case MB_REG_ERROR:
      *value = system_error;
      return MB_NO_ERROR;
    case MB_REG_LB_STATE:
      *value = lb_get_state();
      return MB_NO_ERROR;
    case MB_REG_CONFIG_CHARGER_LIMIT:
      *value = config.lb_config.charger_limit;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_NUMBER_OF_PHASES:
      *value = config.lb_config.number_of_phases;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_ALARM_LIMIT:
      *value = config.lb_config.alarm_limit;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_ALARM_LIMIT_WAIT_TIME:
      *value = config.lb_config.alarm_limit_wait_time;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_ALARM_LIMIT_CHANGE_AMOUNT:
      *value = config.lb_config.alarm_limit_change_amount;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_UPPER_LIMIT:
      *value = config.lb_config.upper_limit;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_UPPER_LIMIT_WAIT_TIME:
      *value = config.lb_config.upper_limit_wait_time;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_UPPER_LIMIT_CHANGE_AMOUNT:
      *value = config.lb_config.upper_limit_change_amount;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_LOWER_LIMIT:
      *value = config.lb_config.lower_limit;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_LOWER_LIMIT_WAIT_TIME:
      *value = config.lb_config.lower_limit_wait_time;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_LOWER_LIMIT_CHANGE_AMOUNT:
      *value = config.lb_config.lower_limit_change_amount;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_FALLBACK_LIMIT:
      *value = config.lb_config.fallback_limit;
      return MB_NO_ERROR;
    case MB_REG_CONFIG_FALLBACK_LIMIT_WAIT_TIME:
      *value = config.lb_config.fallback_limit_wait_time;
      return MB_NO_ERROR;
    default:
      return MB_ERROR_ILLEGAL_DATA_ADDRESS;
  }
}

static enum mb_result read_holding_registers(uint16_t start, uint16_t count) {
  uint16_t val;
  for (int i = 0; i < count; i++) {
    if (read_single_holding_register(start + i, &val) == MB_NO_ERROR) {
      mb_server_add_response(&mb_server_ctx, val);
    } else {
      return MB_ERROR_ILLEGAL_DATA_ADDRESS;
    }
  }
  return MB_NO_ERROR;
}

static void led_task() {
  static bool on = false;
  static absolute_time_t led_timer;
  static int pulse_counter = 0;

  uint8_t led_pulse_mode = lb_get_state();

  if (absolute_time_diff_us(led_timer, get_absolute_time()) > 0) {
    if (pulse_counter == 0) {
      pulse_counter = led_pulse_mode == 4 ? 1 : (led_pulse_mode + 1);
      led_timer = make_timeout_time_ms(1000);
    } else if (on) {
      board_led_off();
      on = false;
      led_timer = make_timeout_time_ms(led_pulse_mode == 4 ? 0 : 300);
      pulse_counter--;
    } else {
      board_led_on();
      on = true;
      led_timer = make_timeout_time_ms(led_pulse_mode == 4 ? 1000 : 100);
    }
  }
}

static void mb_client_tx_request(uint8_t* data, size_t size) {
  mb_client_send_raw(&mb_client_ctx, data, size);
}

static void setup_uarts(void) {
  gpio_init(MB_DE_PIN);
  gpio_set_dir(MB_DE_PIN, GPIO_OUT);
  gpio_put(MB_DE_PIN, 0);

  uart_init(DSMR_UART, DSMR_UART_BAUD);
  uart_init(MB_UART, MB_UART_BAUD);

  gpio_set_function(DSMR_RX_PIN, GPIO_FUNC_UART);
  gpio_set_function(MB_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(MB_RX_PIN, GPIO_FUNC_UART);

  gpio_pull_down(MB_RX_PIN);
  gpio_pull_down(DSMR_RX_PIN);

  uart_set_fifo_enabled(DSMR_UART, false);
  uart_set_fifo_enabled(MB_UART, false);

  irq_set_exclusive_handler(DSMR_UART_IRQ, on_dsmr_rx);
  irq_set_exclusive_handler(MB_UART_IRQ, on_mb_rx);
  irq_set_enabled(DSMR_UART_IRQ, true);
  irq_set_enabled(MB_UART_IRQ, true);

  uart_set_irq_enables(DSMR_UART, true, false);
  uart_set_irq_enables(MB_UART, true, false);
}

int main(void) {
  board_init();
  tud_init(TUD_OPT_RHPORT);
  stdio_usb_init();

  if (watchdog_caused_reboot()) {
    printf("# Rebooted by watchdog!\n");
  } else {
    printf("# Clean boot\n");
  }

  watchdog_enable(100, 1);

  setup_uarts();

  config_load();

  lb_init(&config.lb_config, limit_charger);

  dsmr_init(dsmr_update, dsmr_forward);

  struct mb_server_cb server_cb = {
      .get_tick_ms = mb_get_tick_ms,
      .tx = mb_server_tx,
      .write_single_register = write_single_holding_register,
      .read_holding_registers = read_holding_registers,
      .write_multiple_registers = write_holding_registers,
      .raw_rx = mb_client_tx_request,
  };
  mb_server_init(&mb_server_ctx, config.address, &server_cb);

  struct mb_client_cb client_cb = {
      .get_tick_ms = mb_get_tick_ms,
      .tx = mb_client_tx,
      .status = mb_client_status,
      .raw_rx = mb_server_tx,
  };
  mb_client_init(&mb_client_ctx, &client_cb);

  printf("# P1 Load balancing modbus controller\r\n");

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
  for (;;) {
    tud_task();
    dsmr_task();
    mb_server_task(&mb_server_ctx);
    mb_client_task(&mb_client_ctx);
    lb_task(mb_get_tick_ms());
    led_task();
    watchdog_update();
  }
#pragma clang diagnostic pop
}
