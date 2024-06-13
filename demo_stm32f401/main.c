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

uintptr_t ac_trap_handler(uint32_t id, uintptr_t frame) {
    return ac_actor_exception();
}

static struct ac_channel_t g_chan;
static struct ac_message_pool_t g_pool;

struct ac_channel_t* ac_channel_validate(
    struct ac_actor_t* actor, 
    unsigned int handle
) {
    return (handle == 0) ? &g_chan : 0;
}

struct ac_message_pool_t* ac_pool_validate(
    struct ac_actor_t* actor, 
    unsigned int handle
) {
    return (handle == 0) ? &g_pool : 0;
}

int main(void) {
    static struct ac_actor_t g_handler;
    static struct ac_actor_t g_sender;
    static alignas(1024) uint8_t stack0[1024];
    static alignas(sizeof(struct led_msg_t)) struct led_msg_t g_storage[3];

    /* setup clocks and other hardware stuff... */

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

    /* enable LED */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER |= GPIO_MODER_MODER13_0;

    /* enable all exceptions */
    SCB->SHCSR |= ( 7 << 16 );
    
    /* enable MPU with default memory map */
    MPU->CTRL = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();
    
    /* base address of flash and its size */
    ac_context_init(0x08000000, 0x40000);

    /* both actors share the same priority so there's only one stack */
    g_ac_context.stacks[0].addr = (uintptr_t)stack0;
    g_ac_context.stacks[0].size = sizeof(stack0);

    /* create global objects, 1 is used as unique type id, any value may be used */
    ac_message_pool_init(&g_pool, g_storage, sizeof(g_storage), sizeof(g_storage[0]), 1);
    ac_channel_init(&g_chan, 1);

    /* this chip has 4 priority bits, subpriority is 3:0 */
    NVIC_SetPriorityGrouping(3);

    /* enable two first vectors and set priotity 1, the maximum priority for 
    unprivileged actors */
    NVIC_SetPriority(0, 1);
    NVIC_SetPriority(1, 1);
    NVIC_EnableIRQ(0);
    NVIC_EnableIRQ(1);

    /* create 'controller' actor and allow to access GPIOC */
    ac_actor_init(&g_handler, 0, 0);
    hal_mpu_region_init(&g_handler.granted[AC_REGION_USER], GPIOC_BASE, 64, AC_ATTR_DEV);

    /* create 'sender actor '*/
    ac_actor_init(&g_sender, 1, 1);

    /* initialize tick source for 1 ms */
    SysTick->LOAD  = 84000U - 1U;
    SysTick->VAL   = 0;
    SysTick->CTRL  = 7;

    ac_kernel_start();

    return 0; /* make compiler happy. */
}

