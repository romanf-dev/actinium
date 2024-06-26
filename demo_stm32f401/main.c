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

void* ac_intr_handler(uint32_t vect, void* frame) {
    return _ac_intr_handler(vect, frame);
}

void* ac_svc_handler(uint32_t arg, void* frame) {
    return _ac_svc_handler(arg, frame);
}

void* ac_trap_handler(uint32_t id) {
    return ac_actor_exception();
}

struct mg_context_t g_mg_context;
struct ac_context_t g_ac_context; 

static struct ac_channel_t g_chan[2];

struct ac_channel_t* ac_channel_validate(
    struct ac_actor_t* actor, 
    unsigned int handle,
    bool is_write
) {
    const size_t max_id = sizeof(g_chan) / sizeof(g_chan[0]);
    return (handle < max_id) ? &g_chan[handle] : 0;
}

int main(void) {
    static struct ac_actor_t g_handler;
    static struct ac_actor_t g_sender;
    static alignas(1024) uint8_t stack0[1024];
    static alignas(sizeof(struct led_msg_t)) struct led_msg_t g_storage[3];

    /* 
     * Setup clocks and other hardware stuff...
     */
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

    /* 
     * Enable LED. 
     */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER |= GPIO_MODER_MODER13_0;

    /* 
     * Enable all exceptions.
     */
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
    
    /* 
     * Enable MPU with default memory map.
     */
    MPU->CTRL = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();
    
    /* 
     * Base address of flash and its size.
     */
    ac_context_init(0x08000000, 0x40000);

    /* 
     * Both actors share the same priority so there's only one stack for prio 1.
     */
    ac_context_stack_set(1, sizeof(stack0), stack0);

    /* 
     * Create global objects, 1 is used as unique type id, any value may be used.
     */
    ac_channel_init_ex(&g_chan[0], sizeof(g_storage), g_storage, sizeof(g_storage[0]), 1);
    ac_channel_init(&g_chan[1], 1);

    /* 
     * This chip has 4 priority bits, subpriority is 3:0.
     */
    NVIC_SetPriorityGrouping(3);

    /* 
     * Enable two first vectors and set priotity 1, the maximum priority for 
     * unprivileged actors.
     */
    NVIC_SetPriority(0, 1);
    NVIC_SetPriority(1, 1);
    NVIC_EnableIRQ(0);
    NVIC_EnableIRQ(1);

    /* 
     * Create the 'controller' actor and allow to access GPIOC.
     * The second argument is interrupt vector. Third argument is binary file
     * id. 
     */
    ac_actor_init(&g_handler, 0, 0);
    ac_actor_allow(&g_handler, 64, (void*)GPIOC_BASE, AC_ATTR_DEV);

    /* 
     * Create the 'sender' actor, who sends messages to the 'controller'.
     */
    ac_actor_init(&g_sender, 1, 1);

    /* 
     * Initialize tick source for 1 ms.
     */
    SysTick->LOAD = 84000U - 1U;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    /* 
     * Does not return... 
     */
    ac_kernel_start();

    return 0; /* Make compiler happy. */
}

