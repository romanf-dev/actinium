/** 
  ******************************************************************************
  *  @file   task0.c
  *  @brief  The LED controller (message receiver).
  *****************************************************************************/

#include <stdint.h>
#include "stm32f4xx.h"
#include "actinium.h"
#include "led_msg.h"

static inline void set_led(unsigned int control) {
    if (control == 1) {
        GPIOC->BSRRL = 1U << 13;
    } else {
        GPIOC->BSRRH = 1U << 13;
    }
}

static inline void crash(void) {
    *((volatile int*)0x50000000) = 1;
}

uint32_t main(const struct led_msg_t* msg) {
    static int counter = 0;
    AC_ACTOR_START;
    
    for (;;) {
        AC_AWAIT(act_subscribe_to(0));
        set_led(msg->control);

        AC_AWAIT(act_subscribe_to(0));
        set_led(msg->control);

        if (++counter == 20) {
            crash();
        }
    }

    AC_ACTOR_END;
}

