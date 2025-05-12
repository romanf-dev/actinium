/*
 *  @file   task1.c
 *  @brief  The message sender.
 */

#include <stdint.h>
#include "stm32f4xx.h"
#include "actinium.h"
#include "led_msg.h"

static inline void crash(void) {
    *((volatile int*)0x50000000) = 1;
}

uint32_t main(void) {
    static unsigned int counter = 0;
    static int state = 0;

    struct led_msg_t* msg = ac_try_pop(0);
    msg->control = (state ^= 1);

    ac_push(1);

    if (++counter == 20) {
        crash();
    }

    return ac_sleep_for(1000);
}

