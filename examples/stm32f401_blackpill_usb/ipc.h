/*
 * @file   ipc.h
 * @brief  For the sake of simplicity all the IPC channels and messages are 
 *         described here.
 */

#ifndef IPC_H
#define IPC_H

#include "actinium.h" /* for message header */

enum {
    CHAN_USB_POOL,
    CHAN_USB_SERVER_IN,
    CHAN_USB_SERVER_PRIV,
    CHAN_USB_SERVER_OUT,
    CHAN_APP_POOL,
    CHAN_LED_SERVER_IN,
    CHAN_NUM,
};

enum {
    USB_MSG_SZ = 64,
    LED_MSG_SZ = 32,
};

enum usb_opcode_t {
    USB_INTERRUPT,
    USB_TX_REQUEST,
};

struct usb_msg_header_t {
    struct ac_message_t base;
    enum usb_opcode_t opcode;
    uint8_t payload_len;
};

struct usb_msg_t {
    struct usb_msg_header_t header;
    uint8_t payload[USB_MSG_SZ - sizeof(struct usb_msg_header_t)];
};

enum led_opcode_t {
    LED_OFF,
    LED_ON,
};

struct led_msg_header_t {
    struct ac_message_t base;
    enum led_opcode_t opcode;
};

struct led_msg_t {
    struct led_msg_header_t header;
    uint8_t padding[LED_MSG_SZ - sizeof(struct led_msg_header_t)];
};

_Static_assert(
    (sizeof(struct usb_msg_t) & (sizeof(struct usb_msg_t) - 1)) == 0,
    "usb_msg_t size is not power of 2");

_Static_assert(
    (sizeof(struct led_msg_t) & (sizeof(struct led_msg_t) - 1)) == 0, 
    "led_msg_t size is not power of 2");

#endif

