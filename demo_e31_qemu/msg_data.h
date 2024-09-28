/** 
  ******************************************************************************
  *  @file   msg_data.h
  *  @brief  Message type for actor communication.
  *****************************************************************************/

#ifndef MSG_DATA_H
#define MSG_DATA_H

#include "actinium.h"

enum {
    EXAMPLE_MSG_SZ = 32 - sizeof(struct ac_message_t)
};

struct example_msg_t {
    struct ac_message_t header;
    char text[EXAMPLE_MSG_SZ];
};

_Static_assert(
    (sizeof(struct example_msg_t) & (sizeof(struct example_msg_t) - 1)) == 0, 
    "message size is not power of 2"
);

#endif

