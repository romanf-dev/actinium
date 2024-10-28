/** 
  ******************************************************************************
  *  @file   core.c
  *  @brief  The framework itself is implemented as a header.
  *          This file contains the instantiated functions included from the
  *          header.
  *****************************************************************************/

#include <stdint.h>
#include "actinium.h"

void* ac_intr_handler(uint32_t vect, void* frame) {
    return _ac_intr_handler(vect, frame);
}

void* ac_svc_handler(uint32_t arg, void* frame) {
    return _ac_svc_handler(arg, frame);
}

void* ac_trap_handler(uint32_t id) {
    return ac_actor_exception();
}

