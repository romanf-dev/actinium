/*
 *  @file   serial.h
 *  @brief  Serial output function.
 */

#ifndef SERIAL_H
#define SERIAL_H

#define SERIAL_BASE 0x10013000

static inline void serial_out(const char* str) {
    volatile uint32_t* const txreg = (uint32_t*) 0x10013000;

    while (*str) {

        while (*txreg & (UINT32_C(1) << 31)) {
            ;
        }

        *txreg = *str++;
    }
}

#endif

