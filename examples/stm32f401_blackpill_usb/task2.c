/*
 *  @file   task2.c
 *  @brief  LED server.
 */

#include <stdint.h>
#include "stm32f4xx.h"
#include "actinium.h"
#include "ipc.h"

uint32_t main(const struct led_msg_t* msg) {
    if (msg) {
        switch (msg->header.opcode) {
        case LED_ON:
            GPIOC->BSRR = 1u << 29;
            break;
        case LED_OFF:
            GPIOC->BSRR = 1u << 13;
            break;
        default:
            break;
        }
    } else {
        GPIOC->MODER |= GPIO_MODER_MODER13_0;
    }

    return ac_subscribe_to(CHAN_LED_SERVER_IN);
}

