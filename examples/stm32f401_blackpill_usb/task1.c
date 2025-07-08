/*
 *  @file   task1.c
 *  @brief  The application. Accepts data from USB, sends echos and controls 
 *          the LED via GPIO server.
 */

#include <stdint.h>
#include "stm32f4xx.h"
#include "actinium.h"
#include "ipc.h"

static inline void set_led(uint8_t state) {
    struct led_msg_t* const msg = ac_try_pop(CHAN_APP_POOL);
    
    if (msg) {
        const enum led_opcode_t led_state[] = { LED_OFF, LED_ON };
        msg->header.opcode = led_state[state];
        ac_push(CHAN_LED_SERVER_IN);
    }
}

uint32_t main(struct usb_msg_t* recv_msg) {
    if (recv_msg) {
        const uint8_t control = recv_msg->payload[0];
        recv_msg->header.opcode = USB_TX_REQUEST;
        ac_push(CHAN_USB_SERVER_IN); /* send echo */

        if ((control == '0') || (control == '1')) {
            const uint8_t state = control - '0';
            set_led(state);
        }
    }

    return ac_subscribe_to(CHAN_USB_SERVER_OUT);
}

