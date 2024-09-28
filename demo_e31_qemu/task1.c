/** 
  ******************************************************************************
  *  @file   task1.c
  *  @brief  The message sender.
  *****************************************************************************/

#include <stdint.h>
#include <string.h>
#include "actinium.h"
#include "msg_data.h"
#include "serial.h"

static const char g_str[] = "hello!\r\n";

_Static_assert(sizeof(g_str) <= EXAMPLE_MSG_SZ, "too long msg");

uint32_t main(void) {
    AC_ACTOR_START;

    for (;;) {
        AC_AWAIT(act_sleep_for(1000));

        struct example_msg_t* msg = act_try_pop(0);
        
        for (size_t i = 0; i < sizeof(g_str); ++i) {
            msg->text[i] = g_str[i];
        }

        act_push(1);
    }

    AC_ACTOR_END;
}

