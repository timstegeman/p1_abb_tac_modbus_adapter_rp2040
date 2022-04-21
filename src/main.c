//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include <hardware/irq.h>
#include <hardware/uart.h>
#include <hardware/watchdog.h>
#include <pico/stdlib.h>
#include <stdio.h>

#include "dsmr.h"
#include "modbus.h"

#define MB_SLAVE_ADDRESS 1
#define LED_BLINK_TIME   500
#define DATA_VALID_TIME  20000
#define LED_PIN          PICO_DEFAULT_LED_PIN
#define DSMR_UART        uart0
#define DSMR_UART_IRQ    UART0_IRQ
#define DSMR_RX_PIN      17
#define MB_UART          uart1
#define MB_UART_IRQ      UART1_IRQ
#define MB_DE_PIN        7
#define MB_TX_PIN        8
#define MB_RX_PIN        9

#define MB_ACTIVE_IMPORT 0x5000  // 20480
#define MB_VOLTAGE_L1_N  0x5b00  // 23296
#define MB_VOLTAGE_L2_N  0x5b02  // 23298
#define MB_VOLTAGE_L3_N  0x5b04  // 23300
#define MB_CURRENT_L1    0x5b0c  // 23308
#define MB_CURRENT_L2    0x5b0e  // 23310
#define MB_CURRENT_L3    0x5b10  // 23312
#define MB_POWER_TOTAL   0x5b14  // 23316
#define MB_POWER_L1      0x5b16  // 23318
#define MB_POWER_L2      0x5b18  // 23320
#define MB_POWER_L3      0x5b1A  // 23322

#define __swap32 __builtin_bswap32
#define __swap64 __builtin_bswap64

absolute_time_t data_timeout;
absolute_time_t led_timeout;

static struct {
  uint64_t active_import_1;
  uint64_t active_import_2;
  uint32_t voltage_l1;
  uint32_t voltage_l2;
  uint32_t voltage_l3;
  uint32_t current_l1;
  uint32_t current_l2;
  uint32_t current_l3;
  int32_t power_l1;
  int32_t power_l2;
  int32_t power_l3;
} registers;

void mb_tx(uint8_t* data, uint32_t size) {
  gpio_put(MB_DE_PIN, 1);

  uart_write_blocking(MB_UART, data, size);

  // Wait until fifo is drained so we now when to turn off the driver enable pin.
  uart_tx_wait_blocking(MB_UART);
  gpio_put(MB_DE_PIN, 0);
}

uint32_t mb_get_tick_ms(void) {
  return time_us_64() / 1000;
}

void on_dsmr_rx() {
  char c = uart_getc(DSMR_UART);
  dsmr_rx(c);
  putchar(c);
}

void on_mb_rx() {
  mb_rx(uart_getc(MB_UART));
}

static uint8_t mb_read_holding_register(uint16_t addr, uint16_t* reg) {
  uint32_t power_total = registers.power_l1 + registers.power_l2 + registers.power_l3;
  uint64_t active_import = registers.active_import_1 + registers.active_import_2;

  switch (addr) {
    case MB_ACTIVE_IMPORT:
    case MB_ACTIVE_IMPORT + 1:
    case MB_ACTIVE_IMPORT + 2:
    case MB_ACTIVE_IMPORT + 3:
      *reg = (__swap64(active_import) >> (16 * (addr - MB_ACTIVE_IMPORT))) & 0xFFFF;
      break;

    case MB_VOLTAGE_L1_N:
    case MB_VOLTAGE_L1_N + 1:
      *reg = (__swap32(registers.voltage_l1) >> (16 * (addr - MB_VOLTAGE_L1_N))) & 0xFFFF;
      break;

    case MB_VOLTAGE_L2_N:
    case MB_VOLTAGE_L2_N + 1:
      *reg = (__swap32(registers.voltage_l2) >> (16 * (addr - MB_VOLTAGE_L2_N))) & 0xFFFF;
      break;

    case MB_VOLTAGE_L3_N:
    case MB_VOLTAGE_L3_N + 1:
      *reg = (__swap32(registers.voltage_l3) >> (16 * (addr - MB_VOLTAGE_L3_N))) & 0xFFFF;
      break;

    case MB_CURRENT_L1:
    case MB_CURRENT_L1 + 1:
      *reg = (__swap32(registers.current_l1) >> (16 * (addr - MB_CURRENT_L1))) & 0xFFFF;
      break;

    case MB_CURRENT_L2:
    case MB_CURRENT_L2 + 1:
      *reg = (__swap32(registers.current_l2) >> (16 * (addr - MB_CURRENT_L2))) & 0xFFFF;
      break;

    case MB_CURRENT_L3:
    case MB_CURRENT_L3 + 1:
      *reg = (__swap32(registers.current_l3) >> (16 * (addr - MB_CURRENT_L3))) & 0xFFFF;
      break;

    case MB_POWER_TOTAL:
    case MB_POWER_TOTAL + 1:
      *reg = (__swap32(power_total) >> (16 * (addr - MB_POWER_TOTAL))) & 0xFFFF;
      break;

    case MB_POWER_L1:
    case MB_POWER_L1 + 1:
      *reg = (__swap32(registers.power_l1) >> (16 * (addr - MB_POWER_L1))) & 0xFFFF;
      break;

    case MB_POWER_L2:
    case MB_POWER_L2 + 1:
      *reg = (__swap32(registers.power_l2) >> (16 * (addr - MB_POWER_L2))) & 0xFFFF;
      break;

    case MB_POWER_L3:
    case MB_POWER_L3 + 1:
      *reg = (__swap32(registers.power_l3) >> (16 * (addr - MB_POWER_L3))) & 0xFFFF;
      break;
    default:
      return MB_ERROR_ILLEGAL_DATA_ADDRESS;
  }
  return MB_NO_ERROR;
}

uint8_t mb_read_holding_registers(uint16_t start, uint16_t count) {
  uint16_t val;
  for (int i = 0; i < count; i++) {
    if (mb_read_holding_register(start + i, &val) == MB_NO_ERROR) {
      mb_response_add(val);
    } else {
      return MB_ERROR_ILLEGAL_DATA_ADDRESS;
    }
  }
  return MB_NO_ERROR;
}

void dsmr_update(dsmr_msg_t obj, float value) {
  switch (obj) {
    case MSG_ACTIVE_IMPORT_1:
      registers.active_import_1 = (uint64_t)(value * 10000);
      break;
    case MSG_ACTIVE_IMPORT_2:
      registers.active_import_2 = (uint64_t)(value * 10000);
      break;
    case MSG_VOLTAGE_L1:
      registers.voltage_l1 = (uint32_t)(value * 10);
      break;
    case MSG_VOLTAGE_L2:
      registers.voltage_l2 = (uint32_t)(value * 10);
      break;
    case MSG_VOLTAGE_L3:
      registers.voltage_l3 = (uint32_t)(value * 10);
      break;
    case MSG_CURRENT_L1:
      registers.current_l1 = (uint32_t)(value * 100);
      break;
    case MSG_CURRENT_L2:
      registers.current_l2 = (uint32_t)(value * 100);
      break;
    case MSG_CURRENT_L3:
      registers.current_l3 = (uint32_t)(value * 100);
      break;
    case MSG_POWER_L1:
      registers.power_l1 = (int32_t)(value * 100000);  // 1 kW to 0.01 W
      break;
    case MSG_POWER_L2:
      registers.power_l2 = (int32_t)(value * 100000);  // 1 kW to 0.01 W
      break;
    case MSG_POWER_L3:
      registers.power_l3 = (int32_t)(value * 100000);  // 1 kW to 0.01 W
      break;
    default:
      break;
  }

  gpio_put(LED_PIN, 1);
  data_timeout = make_timeout_time_ms(DATA_VALID_TIME);
  led_timeout = make_timeout_time_ms(LED_BLINK_TIME);
}

void setup(void) {
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

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

  dsmr_init();
  mb_init(MB_SLAVE_ADDRESS);
}

int main(void) {
  stdio_init_all();
  printf("# P1 ABB Modbus Adapter\r\n");

  if (watchdog_caused_reboot()) {
    printf("# Rebooted by Watchdog!\n");
  } else {
    printf("# Clean boot\n");
  }

  watchdog_enable(100, 1);

  setup();

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
  for (;;) {
    dsmr_process();

    if (absolute_time_diff_us(data_timeout, get_absolute_time()) <= 0) {
      //  Only process modbus data when we have up-to-date DSMR data
      mb_process();
    }

    if (absolute_time_diff_us(led_timeout, get_absolute_time()) > 0) {
      gpio_put(LED_PIN, 0);
    }

    watchdog_update();
  }
#pragma clang diagnostic pop
}
