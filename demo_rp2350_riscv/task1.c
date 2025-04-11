/** 
  ******************************************************************************
  *  @file   task1.c
  *  @brief  The message sender.
  *****************************************************************************/

#include <stdint.h>
#include <string.h>
#include "actinium.h"
#include "led_msg.h"

static unsigned int g_led = 0;

uint32_t main(void) {
    AC_ACTOR_START;

    for (;;) {
        AC_AWAIT(ac_sleep_for(100));

        struct led_msg_t* msg = ac_try_pop(0);
        msg->control = (g_led ^= 1);
        ac_push(1);
    }

    AC_ACTOR_END;
}

