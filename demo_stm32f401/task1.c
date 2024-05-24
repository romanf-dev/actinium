/** 
  ******************************************************************************
  *  @file   task1.c
  *  @brief  The message sender.
  *****************************************************************************/

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
    AC_ACTOR_START;
    
    for (;;) {
        AC_AWAIT(act_sleep_for(50));

        struct led_msg_t* msg = act_alloc(0);
        msg->control = (state ^= 1);

        act_push(0);

        if (++counter == 50) {
            crash();
        }
    }

    AC_ACTOR_END;
}

