/** 
  ******************************************************************************
  *  @file   task0.c
  *  @brief  Receiver task which prints the data. It crashes at every 3rd msg.
  *****************************************************************************/

#include <stdint.h>
#include "actinium.h"
#include "msg_data.h"
#include "serial.h"

static inline void crash(void) {
    *((volatile int*)0x50000000) = 1;
}

uint32_t main(const struct example_msg_t* msg) {
    static int counter = 0;
    AC_ACTOR_START;
    
    for (;;) {
        AC_AWAIT(act_subscribe_to(1));
        serial_out(msg->text);

        if (++counter == 3) {
            crash();
        }
    }

    AC_ACTOR_END;
}

