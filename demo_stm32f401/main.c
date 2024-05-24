/** 
  ******************************************************************************
  *  @file   main.c
  *  @brief  Demo application for the Actinium framework. The kernel part.
  *****************************************************************************/

#include <stdint.h>
#include <stdalign.h>
#include <stdnoreturn.h>
#include "stm32f4xx.h"
#include "actinium.h"
#include "led_msg.h"

static noreturn void Error_Handler(void) {
    __disable_irq();
    for(;;);
}

void HardFault_Handler(void) {
    Error_Handler();
}

void SysTick_Handler(void) {
    mg_context_tick();
}

void __assert_func(
    const char *file, 
    int line, 
    const char *func, 
    const char *expr
) {
    Error_Handler();
}

uintptr_t ac_intr_handler(uint32_t vect, uintptr_t old_frame) {
    return _ac_intr_handler(vect, old_frame);
}

uintptr_t ac_svc_handler(uint32_t r0, uintptr_t old_frame) {
    return _ac_svc_handler(r0, old_frame);
}

struct mg_context_t g_mg_context;
struct ac_context_t g_ac_context; 

void MemManage_Handler(void) {
    const uintptr_t frame = ac_actor_exception();
    __set_PSP(frame);
}

void UsageFault_Handler(void) {
    const uintptr_t frame = ac_actor_exception();
    __set_PSP(frame);
}

void BusFault_Handler(void) {
    const uintptr_t frame = ac_actor_exception();
    __set_PSP(frame);
}

static alignas(sizeof(struct led_msg_t)) struct led_msg_t g_storage[3];
static struct ac_channel_t g_chan;
static struct ac_message_pool_t g_pool;

struct ac_channel_t* ac_channel_validate(
    struct ac_actor_t* actor, 
    unsigned int handle
) {
    return &g_chan;
}

struct ac_message_pool_t* ac_pool_validate(
    struct ac_actor_t* actor, 
    unsigned int handle
) {
    return &g_pool;
}

int main(void) {
    static struct ac_actor_t g_handler;
    static struct ac_actor_t g_sender;

    RCC->CR |= RCC_CR_HSEON;            
    while((RCC->CR & RCC_CR_HSERDY) == 0) {
        ;
    }
    
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_2WS;

    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
    RCC->PLLCFGR = (0x27U << 24) | RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLP_0 | (336U << 6) | 25U;

    RCC->CR |= RCC_CR_PLLON;
    while((RCC->CR & RCC_CR_PLLRDY) == 0) {
        ;
    }
    
    RCC->CFGR = (RCC->CFGR | RCC_CFGR_SW_PLL) & ~RCC_CFGR_SW_HSE;
    while((RCC->CFGR & RCC_CFGR_SWS_PLL) == 0) {
        ;
    }

    RCC->CR &= ~RCC_CR_HSION;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER |= GPIO_MODER_MODER13_0;

    SCB->SHCSR |= ( 7 << 16 );
    
    MPU->RNR = 0;
    MPU->RBAR = 0;
    MPU->RASR = (3 << 24) | (31 << 1) | 1 | 3 << 17u;
    MPU->CTRL = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();
    
    ac_context_init();
    g_ac_context.stacks[0].addr = 0x2000ec00;
    g_ac_context.stacks[0].size = 1024;

    ac_message_pool_init(&g_pool, g_storage, sizeof(g_storage), sizeof(g_storage[0]), 1);
    ac_channel_init(&g_chan, 1);

    NVIC_SetPriorityGrouping(3);
    NVIC_SetPriority(0, 1);
    NVIC_SetPriority(1, 1);
    NVIC_EnableIRQ(0);
    NVIC_EnableIRQ(1);

    ac_actor_init(&g_handler, 0, 0);
    hal_mpu_region_init(&g_handler.granted[AC_REGION_USER], 0x40020800, 1024, true);

    ac_actor_init(&g_sender, 1, 1);

    SysTick->LOAD  = 84000U - 1U;
    SysTick->VAL   = 0;
    SysTick->CTRL  = 7;

    ac_kernel_start();

    return 0; /* make compiler happy. */
}

