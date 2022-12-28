//  SPDX-FileCopyrightText: 2022 Tim Stegeman <tim.stegeman@gmail.com>
//  SPDX-License-Identifier: MIT

#include <pico/bootrom.h>
#include <pico/stdio_usb.h>

#include "tusb.h"

#define USB_VID (0x2E8A)  // Raspberry Pi
#define USB_PID (0x000a)  // Raspberry Pi Pico SDK CDC

tusb_desc_device_t const desc_device = {.bLength = sizeof(tusb_desc_device_t),
                                        .bDescriptorType = TUSB_DESC_DEVICE,
                                        .bcdUSB = 0x0200,
                                        .bDeviceClass = TUSB_CLASS_MISC,
                                        .bDeviceSubClass = MISC_SUBCLASS_COMMON,
                                        .bDeviceProtocol = MISC_PROTOCOL_IAD,
                                        .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
                                        .idVendor = USB_VID,
                                        .idProduct = USB_PID,
                                        .bcdDevice = 0x0100,
                                        .iManufacturer = 0x01,
                                        .iProduct = 0x02,
                                        .iSerialNumber = 0x03,
                                        .bNumConfigurations = 0x01};

uint8_t const* tud_descriptor_device_cb(void) {
  return (uint8_t const*)&desc_device;
}

enum { ITF_NUM_CDC_0 = 0, ITF_NUM_CDC_0_DATA, ITF_NUM_CDC_1, ITF_NUM_CDC_1_DATA, ITF_NUM_TOTAL };

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_CDC * TUD_CDC_DESC_LEN)

#define EPNUM_CDC_0_NOTIF 0x81
#define EPNUM_CDC_0_OUT   0x02
#define EPNUM_CDC_0_IN    0x82
#define EPNUM_CDC_1_NOTIF 0x83
#define EPNUM_CDC_1_OUT   0x04
#define EPNUM_CDC_1_IN    0x84

static const uint8_t usbd_desc_cfg[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, 4, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_OUT, EPNUM_CDC_1_IN, 64),
};

const uint8_t* tud_descriptor_configuration_cb(__unused uint8_t index) {
  return usbd_desc_cfg;
}

#define USBD_STR_0       (0x00)
#define USBD_STR_MANUF   (0x01)
#define USBD_STR_PRODUCT (0x02)
#define USBD_STR_SERIAL  (0x03)
#define USBD_STR_CDC     (0x04)

static const char* const usbd_desc_str[] = {
    [USBD_STR_MANUF] = "Raspberry Pi", [USBD_STR_PRODUCT] = "Pico",       [USBD_STR_SERIAL] = "654321",
    [USBD_STR_CDC] = "Board CDC1",     [USBD_STR_CDC + 1] = "Board CDC2",
};

const uint16_t* tud_descriptor_string_cb(uint8_t index, __unused uint16_t langid) {
#define DESC_STR_MAX (20)
  static uint16_t desc_str[DESC_STR_MAX];

  // Assign the SN using the unique flash id //TODO
  //    if (!usbd_serial_str[0]) {
  //        pico_get_unique_board_id_string(usbd_serial_str, sizeof(usbd_serial_str));
  //    }

  uint8_t len;
  if (index == 0) {
    desc_str[1] = 0x0409;  // supported language is English
    len = 1;
  } else {
    if (index >= sizeof(usbd_desc_str) / sizeof(usbd_desc_str[0])) {
      return NULL;
    }
    const char* str = usbd_desc_str[index];
    for (len = 0; len < DESC_STR_MAX - 1 && str[len]; ++len) {
      desc_str[1 + len] = str[len];
    }
  }

  // first byte is length (including header), second byte is string type
  desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));

  return desc_str;
}

// Support for default BOOTSEL reset by changing baud rate
void tud_cdc_line_coding_cb(__unused uint8_t itf, cdc_line_coding_t const* p_line_coding) {
  if (p_line_coding->bit_rate == PICO_STDIO_USB_RESET_MAGIC_BAUD_RATE) {
    reset_usb_boot(0, PICO_STDIO_USB_RESET_BOOTSEL_INTERFACE_DISABLE_MASK);
  }
}