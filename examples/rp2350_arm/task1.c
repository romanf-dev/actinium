/*
 *  @file   task1.c
 *  @brief  The message sender.
 */

#include <stdint.h>
#include <string.h>
#include "actinium.h"
#include "led_msg.h"

uint32_t main(void) {
    static unsigned int s_led = 0;
    struct led_msg_t* msg = ac_try_pop(0);
    msg->control = (s_led ^= 1);
    ac_push(1);

    return ac_sleep_for(100);
}

