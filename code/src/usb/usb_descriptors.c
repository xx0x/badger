/*
MIT License

Copyright (c) 2026 Vaclav Mach

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/**
 * @file usb_descriptors.c
 * @brief USB descriptors for the Badger's USB MIDI device interface.
 */

#include "py32f0xx.h"
#include "tusb.h"

#include <string.h>

#define USB_VID 0xCafe
#define USB_PID 0x4014

static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

enum
{
    ITF_NUM_MIDI = 0,
    ITF_NUM_MIDI_STREAMING,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)

/* The PY32 shares the FIFO between IN and OUT of the same endpoint number, so IN and OUT
 * must use different endpoint numbers (CFG_TUD_ENDPOINT_ONE_DIRECTION_ONLY, see tusb_mcu.h). */
#define EPNUM_MIDI_OUT 0x01
#define EPNUM_MIDI_IN 0x02

static uint8_t const desc_fs_configuration[] = {
    /* Config number, interface count, string index, total length, attribute, power in mA */
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    /* Interface number, string index, EP Out & EP In address, EP size */
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, EPNUM_MIDI_OUT, (0x80 | EPNUM_MIDI_IN), 64)};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index; /* for multiple configurations */
    return desc_fs_configuration;
}

enum
{
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};

static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, /* 0: supported language is English (0x0409) */
    "xx0x",                     /* 1: Manufacturer */
    "The Badger",               /* 2: Product */
    NULL,                       /* 3: Serial, built from the MCU unique ID */
};

static uint16_t desc_str[32 + 1];

/**
 * @brief Renders the 96-bit MCU unique ID as a 24-character hex serial number.
 */
static size_t usb_serial_from_uid(uint16_t *utf16, size_t max_chars)
{
    static char const kHexDigits[] = "0123456789ABCDEF";
    volatile uint32_t const *uid = (volatile uint32_t const *)UID_BASE;

    size_t count = 0;
    for (size_t word = 0; word < 3U; ++word)
    {
        const uint32_t value = uid[word];
        for (size_t nibble = 0; nibble < 8U; ++nibble)
        {
            if (count >= max_chars)
            {
                return count;
            }
            const uint32_t shift = 28U - (nibble * 4U);
            utf16[count] = (uint16_t)kHexDigits[(value >> shift) & 0x0FU];
            ++count;
        }
    }

    return count;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    size_t chr_count;
    const size_t max_count = (sizeof(desc_str) / sizeof(desc_str[0])) - 1U; /* -1 for the header */

    switch (index)
    {
    case STRID_LANGID:
        memcpy(&desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
        break;

    case STRID_SERIAL:
        chr_count = usb_serial_from_uid(desc_str + 1, max_count);
        break;

    default:
        if (index >= (sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
        {
            return NULL;
        }

        {
            char const *str = string_desc_arr[index];

            chr_count = strlen(str);
            if (chr_count > max_count)
            {
                chr_count = max_count;
            }

            /* Convert ASCII string into UTF-16 */
            for (size_t i = 0; i < chr_count; ++i)
            {
                desc_str[1 + i] = (uint16_t)str[i];
            }
        }
        break;
    }

    /* First byte is length (including header), second byte is string type */
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2U * chr_count + 2U));

    return desc_str;
}
