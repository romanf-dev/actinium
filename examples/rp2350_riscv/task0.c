/** 
  ******************************************************************************
  *  @file   task0.c
  *  @brief  Task that switches the LED on each incoming message.
  *          The task crashes by invalid memory reference every 5th call.
  *****************************************************************************/

#include <stdint.h>
#include "actinium.h"
#include <hardware/structs/sio.h>
#include "led_msg.h"

static inline void crash(void) {
    *((volatile int*)0x70000000) = 1;
}

uint32_t main(const struct led_msg_t* msg) {
    static unsigned counter = 0;
    AC_ACTOR_START;
    
    for (;;) {
        AC_AWAIT(ac_subscribe_to(1));
        sio_hw->gpio_togl = 1 << 25;

        if (++counter == 5) {
            crash();
        }
    }

    AC_ACTOR_END;
}

