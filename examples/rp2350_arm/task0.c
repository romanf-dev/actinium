/** 
  ******************************************************************************
  *  @file   task0.c
  *  @brief  Task that switches the LED on each incoming message.
  *          The task crashes by invalid memory reference every 5th call.
  *****************************************************************************/

#include <stdint.h>
#include "actinium.h"
#include "RP2350.h"
#include "led_msg.h"

static inline void crash(void) {
    *((volatile int*)0x70000000) = 1;
}

uint32_t main(const struct led_msg_t* msg) {
    static unsigned counter = 0;
    
    if (msg) {
        SIO->GPIO_OUT_XOR = 1U << 25;;

        if (++counter == 5) {
            crash();
        }
    }

    return ac_subscribe_to(1);
}

