//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include <hardware/irq.h>
#include <hardware/uart.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <pico/stdlib.h>
#include <stdio.h>

#include "bsp/board.h"
#include "dsmr.h"
#include "loadbalancer.h"
#include "modbus_client.h"
#include "modbus_server.h"
#include "tusb.h"

#define MB_SLAVE_ADDRESS 10
#define LED_BLINK_TIME   500
#define LED_PIN          PICO_DEFAULT_LED_PIN
#define DSMR_UART        uart0
#define DSMR_UART_IRQ    UART0_IRQ
#define DSMR_RX_PIN      17
#define MB_UART          uart1
#define MB_UART_IRQ      UART1_IRQ
#define MB_DE_PIN        7
#define MB_TX_PIN        8
#define MB_RX_PIN        9

static const uint16_t grid_limit = 25000;  // TODO Configure this over modbus
static struct lb_config my_config = {.charger_limit = 16000,
                                     .number_of_phases = 3,
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

static absolute_time_t led_timeout;
static struct mb_server_context mb_server_ctx;
static struct mb_client_context mb_client_ctx;

static uint8_t temp_buffer[MB_MAX_RTU_FRAME_SIZE];
static uint32_t temp_buffer_len;

static void mb_client_tx(uint8_t* data, uint32_t size) {
  gpio_put(MB_DE_PIN, 1);
  uart_write_blocking(MB_UART, data, size);

  // Wait until fifo is drained so we now when to turn off the driver enable pin.
  uart_tx_wait_blocking(MB_UART);
  gpio_put(MB_DE_PIN, 0);
}

static void mb_client_tx_pass_thru(uint8_t* data, uint32_t size) {
  if (mb_client_busy(&mb_client_ctx)) {
    // store and retry later
    memcpy(temp_buffer, data, size);
    temp_buffer_len = size;
    return;
  }
  mb_client_tx(data, size);
}

static void mb_server_tx(uint8_t* data, uint32_t size) {
  tud_cdc_n_write(1, data, size);
  tud_cdc_n_write_flush(1);
}

static uint32_t mb_get_tick_ms(void) {
  return time_us_64() / 1000;
}

static void on_dsmr_rx() {
  char c = uart_getc(DSMR_UART);
  dsmr_rx(c);
  putchar(c);
}

static void on_mb_rx() {
  tud_cdc_n_write_char(1, uart_getc(MB_UART));
  mb_client_rx(&mb_client_ctx, uart_getc(MB_UART));
}

void tud_cdc_rx_cb(uint8_t i) {
  char c;
  if (i == 1) {
    while (tud_cdc_n_available(i)) {
      if (tud_cdc_n_read(i, &c, 1)) {
        mb_server_rx(&mb_server_ctx, c);
      }
    }
  }
}

static void dsmr_update(dsmr_msg_t obj, float value) {
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

  gpio_put(LED_PIN, 1);
  led_timeout = make_timeout_time_ms(LED_BLINK_TIME);
}

static void limit_charger(uint16_t current) {
  // Only support for ABB Terra AC charger
  mb_client_write_single_register(&mb_client_ctx, 1, 0x4100, current);
}

static void setup(void) {
  board_init();
  tud_init(TUD_OPT_RHPORT);

  gpio_init(MB_DE_PIN);
  gpio_set_dir(MB_DE_PIN, GPIO_OUT);
  gpio_put(MB_DE_PIN, 0);

  uart_init(DSMR_UART, 115200);  // P1
  uart_init(MB_UART, 9600);      // Modbus

  gpio_set_function(DSMR_RX_PIN, GPIO_FUNC_UART);
  gpio_set_function(MB_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(MB_RX_PIN, GPIO_FUNC_UART);

  uart_set_fifo_enabled(DSMR_UART, false);
  uart_set_fifo_enabled(MB_UART, false);

  irq_set_exclusive_handler(DSMR_UART_IRQ, on_dsmr_rx);
  irq_set_exclusive_handler(MB_UART_IRQ, on_mb_rx);
  irq_set_enabled(DSMR_UART_IRQ, true);
  irq_set_enabled(MB_UART_IRQ, true);

  uart_set_irq_enables(DSMR_UART, true, false);
  uart_set_irq_enables(MB_UART, true, false);
}

static enum mb_result write_single_register(uint16_t start, uint16_t value) {
  if (start == 0x4100) {
    lb_set_charger_limit(value);
    return MB_NO_ERROR;
  }
  return MB_ERROR_ILLEGAL_DATA_ADDRESS;
}

static void led_task() {
  if (absolute_time_diff_us(led_timeout, get_absolute_time()) > 0) {
    gpio_put(LED_PIN, 0);
  }
}

static void retry_modbus_request_task(void) {
  if (temp_buffer_len == 0) {
    return;
  }
  if (mb_client_busy(&mb_client_ctx)) {
    return;
  }
  mb_client_tx(temp_buffer, temp_buffer_len);
  temp_buffer_len = 0;
}

int main(void) {
  setup();
  stdio_init_all();
  printf("# P1 Load balancing modbus adapter\r\n");

  if (watchdog_caused_reboot()) {
    printf("# Rebooted by watchdog!\n");
  } else {
    printf("# Clean boot\n");
  }

  watchdog_enable(100, 1);

  lb_init(&my_config, &limit_charger);

  struct mb_server_cb server_cb = {.get_tick_ms = mb_get_tick_ms,
                                   .tx = mb_server_tx,
                                   .write_single_register = write_single_register,
                                   .pass_thru = mb_client_tx_pass_thru};
  mb_server_init(&mb_server_ctx, MB_SLAVE_ADDRESS, &server_cb);

  struct mb_client_cb client_cb = {.get_tick_ms = mb_get_tick_ms, .tx = mb_client_tx, .pass_thru = mb_server_tx};
  mb_client_init(&mb_client_ctx, &client_cb);

  dsmr_init(dsmr_update);

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
  for (;;) {
    tud_task();
    dsmr_task();
    mb_server_task(&mb_server_ctx);
    mb_client_task(&mb_client_ctx);
    lb_task(mb_get_tick_ms());
    led_task();
    retry_modbus_request_task();
    watchdog_update();
  }
#pragma clang diagnostic pop
}
