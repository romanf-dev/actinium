/** 
  ******************************************************************************
  *  @file   led_msg.h
  *  @brief  Message type for actor communication.
  *****************************************************************************/

#ifndef LED_MSG_H
#define LED_MSG_H

#include "actinium.h"

struct led_msg_t {
    struct ac_message_t header;
    unsigned int control;
    unsigned int padding[3];
};

_Static_assert(sizeof(struct led_msg_t) == 32, "wrong msg size");

#endif

