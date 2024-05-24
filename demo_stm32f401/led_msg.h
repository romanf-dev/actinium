/** 
  ******************************************************************************
  *  @file   led_msg.h
  *  @brief  Message type for actor communication.
  *****************************************************************************/

#ifndef LED_MSG_H
#define LED_MSG_H

#include "actinium.h"

struct led_msg_t {
    ac_message_t header;
    unsigned int control;
    uint32_t padding[4];
};

_Static_assert(
    (sizeof(struct led_msg_t) & (sizeof(struct led_msg_t) - 1)) == 0, 
    "message size is not power of 2"
);

#endif

