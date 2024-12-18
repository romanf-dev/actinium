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
        AC_AWAIT(ac_sleep_for(1000));

        struct led_msg_t* msg = ac_try_pop(0);
        msg->control = (state ^= 1);

        ac_push(1);

        if (++counter == 20) {
            crash();
        }
    }

    AC_ACTOR_END;
}

