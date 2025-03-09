/** 
  ******************************************************************************
  *  @file   pmp.c
  *  @brief  PMP management functions.
  *****************************************************************************/

#include <stdint.h>
#include "ac_port.h"

#define csrr(csr, data) asm volatile ("csrr %0, " #csr : "=r" (data))
#define csrw(csr, data) asm volatile ("csrw " #csr ", %0" : : "r" (data))

void ac_pmp_update_entry(unsigned i, uint32_t addr, uint32_t attr) {
    uint32_t pmpcfg[2];
    uint8_t* bytes = (uint8_t*) &pmpcfg[0];

    csrr(pmpcfg0, pmpcfg[0]);
    csrr(pmpcfg1, pmpcfg[1]);
    bytes[i] = attr;
    csrw(pmpcfg0, pmpcfg[0]);
    csrw(pmpcfg1, pmpcfg[1]); 

    switch (i) {
        case 0: csrw(pmpaddr0, addr); break;
        case 1: csrw(pmpaddr1, addr); break;
        case 2: csrw(pmpaddr2, addr); break;
        case 3: csrw(pmpaddr3, addr); break;
        case 4: csrw(pmpaddr4, addr); break;
        case 5: csrw(pmpaddr5, addr); break;
        case 6: csrw(pmpaddr6, addr); break;
        case 7: csrw(pmpaddr7, addr); break;
        default: break;
    }
}

void ac_pmp_reprogram(unsigned sz, const struct ac_port_region_t* regions) {
    uint32_t pmpcfg[2];
    uint8_t* bytes = (uint8_t*) &pmpcfg[0];

    csrr(pmpcfg0, pmpcfg[0]);
    csrr(pmpcfg1, pmpcfg[1]);

    switch ((sz - 1) & 7) {
        case 7: csrw(pmpaddr7, regions[7].pmpaddr); bytes[7] = regions[7].attr;
        case 6: csrw(pmpaddr6, regions[6].pmpaddr); bytes[6] = regions[6].attr;
        case 5: csrw(pmpaddr5, regions[5].pmpaddr); bytes[5] = regions[5].attr;
        case 4: csrw(pmpaddr4, regions[4].pmpaddr); bytes[4] = regions[4].attr;
        case 3: csrw(pmpaddr3, regions[3].pmpaddr); bytes[3] = regions[3].attr;
        case 2: csrw(pmpaddr2, regions[2].pmpaddr); bytes[2] = regions[2].attr;
        case 1: csrw(pmpaddr1, regions[1].pmpaddr); bytes[1] = regions[1].attr;
        case 0: csrw(pmpaddr0, regions[0].pmpaddr); bytes[0] = regions[0].attr;
    }

    csrw(pmpcfg0, pmpcfg[0]);
    csrw(pmpcfg1, pmpcfg[1]);    
}

