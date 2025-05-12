/*
 *  @file   task0.c
 *  @brief  Receiver task which prints the data. It crashes at every 3rd msg.
 */

#include <stdint.h>
#include "actinium.h"
#include "msg_data.h"
#include "serial.h"

static inline void crash(void) {
    *((volatile int*)0x50000000) = 1;
}

uint32_t main(const struct example_msg_t* msg) {
    static int counter = 0;
    
    if (msg) {
        serial_out(msg->text);

        if (++counter == 3) {
            crash();
        }
    }

    return ac_subscribe_to(1);
}

