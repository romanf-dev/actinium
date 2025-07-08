/*
 *  @file   main.c
 *  @brief  Demo application for the Actinium framework. The kernel part.
 */

#include <stdint.h>
#include <stdalign.h>
#include <stdnoreturn.h>
#include "stm32f4xx.h"

#define MG_NVIC_PRIO_BITS 4
#include "actinium.h"
#include "ipc.h"

#if __NVIC_PRIO_BITS != MG_NVIC_PRIO_BITS
#error NVIC priority bits do not match in the kernel and chip header.
#endif

struct mg_context_t g_mg_context;
struct ac_context_t g_ac_context;
static struct ac_channel_t g_chan[CHAN_NUM];

static noreturn void panic(void) {
    __disable_irq();
    for(;;);
}

void HardFault_Handler(void) {
    panic();
}

void SysTick_Handler(void) {
    ac_context_tick();
}

void OTG_FS_IRQHandler(void) {
    struct usb_msg_t* msg = mg_message_alloc(&(g_chan[CHAN_USB_POOL].base));

    if (msg) {
        msg->header.opcode = USB_INTERRUPT;
        USB_OTG_FS->GAHBCFG &= ~1u;
        mg_queue_push(&(g_chan[CHAN_USB_SERVER_IN].base.queue), (void*) msg);
    }
}

void __assert_func(
    const char *file, 
    int line, 
    const char *func, 
    const char *expr
) {
    panic();
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

struct ac_channel_t* ac_channel_validate(
    struct ac_actor_t* actor, 
    unsigned int handle,
    bool is_write
) {
    const size_t max_id = sizeof(g_chan) / sizeof(g_chan[0]);
    return (handle < max_id) ? &g_chan[handle] : 0;
}

void ac_actor_error(struct ac_actor_t* actor) {
    /* just stop the crashed actor */
}

static struct ac_actor_descr_t* descr_by_id(unsigned int task_id) {
    extern const uintptr_t _ac_task_mem_map[]; /* From the linker script. */
    const uintptr_t* const config = (const uintptr_t*) &_ac_task_mem_map;
    const size_t task_num = config[0];
    const struct {
        uintptr_t flash_addr;
        uintptr_t flash_size;
        uintptr_t sram_addr; 
        uintptr_t sram_size;
    } * const slot = (void*) (config + 1);

    _Static_assert(sizeof(*slot) == sizeof(uintptr_t) * 4, "padding");
    assert(task_id < task_num);
    static struct ac_actor_descr_t descr;

    descr.flash_addr = slot[task_id].flash_addr;
    descr.flash_size = slot[task_id].flash_size;
    descr.sram_addr = slot[task_id].sram_addr;
    descr.sram_size = slot[task_id].sram_size;

    return &descr;
}

int main(void) {
    RCC->CR |= RCC_CR_HSEON;            
    while((RCC->CR & RCC_CR_HSERDY) == 0) ;
    
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_2WS;

    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
    RCC->PLLCFGR = (7 << 24) | RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLP_0 | (336 << 6) | 25;

    RCC->CR |= RCC_CR_PLLON;
    while((RCC->CR & RCC_CR_PLLRDY) == 0) ;
    
    RCC->CFGR = (RCC->CFGR | RCC_CFGR_SW_PLL) & ~RCC_CFGR_SW_HSE;
    while((RCC->CFGR & RCC_CFGR_SWS_PLL) == 0) ;

    RCC->CR &= ~RCC_CR_HSION;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIOAEN;
    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

    //
    // Setup USB pins A11/A12.
    //
    GPIOA->MODER |= GPIO_MODER_MODE11_1 | GPIO_MODER_MODE12_1;
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED11_Msk | GPIO_OSPEEDR_OSPEED12_Msk;
    GPIOA->AFR[1] |= (10 << 12) | (10 << 16);

    // 
    // Enable LED. 
    //
    GPIOC->MODER |= GPIO_MODER_MODER13_0;

    // 
    // Enable all exceptions.
    //
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;

    // 
    // Priority 1 is used for the tick and 0 is reserved for traps.
    //
    NVIC_SetPriorityGrouping(3);
    NVIC_SetPriority(SysTick_IRQn, 1);
    NVIC_SetPriority(SVCall_IRQn, 1);
    NVIC_SetPriority(0, 2);
    NVIC_SetPriority(1, 3);
    NVIC_EnableIRQ(0);
    NVIC_EnableIRQ(1);
    NVIC_SetPriority(OTG_FS_IRQn, 2);
    NVIC_EnableIRQ(OTG_FS_IRQn);

    ac_context_init();

    static alignas(1024) uint8_t stack0[1024];
    ac_context_stack_set(2, sizeof(stack0), stack0);

    static alignas(256) uint8_t stack1[256];
    ac_context_stack_set(3, sizeof(stack1), stack1);

    static alignas(sizeof(struct usb_msg_t)) struct usb_msg_t g_usb_msgs[3];
    ac_channel_init_ex(&g_chan[CHAN_USB_POOL], sizeof(g_usb_msgs), g_usb_msgs, sizeof(g_usb_msgs[0]), 1);
    ac_channel_init(&g_chan[CHAN_USB_SERVER_IN], 1);
    ac_channel_init(&g_chan[CHAN_USB_SERVER_PRIV], 1);
    ac_channel_init(&g_chan[CHAN_USB_SERVER_OUT], 1);

    static alignas(sizeof(struct led_msg_t)) struct led_msg_t g_led_msgs[1];
    ac_channel_init_ex(&g_chan[CHAN_APP_POOL], sizeof(g_led_msgs), g_led_msgs, sizeof(g_led_msgs[0]), 2);
    ac_channel_init(&g_chan[CHAN_LED_SERVER_IN], 2);

    static struct ac_actor_t g_usb_server;
    ac_actor_init(&g_usb_server, 0, descr_by_id(0));
    ac_actor_allow(&g_usb_server, 16384u, (void*)USB_OTG_FS_PERIPH_BASE, AC_ATTR_DEV);

    static struct ac_actor_t g_app;
    ac_actor_init(&g_app, 1, descr_by_id(1));

    static struct ac_actor_t g_led;
    ac_actor_init(&g_led, 1, descr_by_id(2));
    ac_actor_allow(&g_led, 64, (void*)GPIOC_BASE, AC_ATTR_DEV);

    SysTick->LOAD = 84000u - 1U;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    ac_kernel_start();
    return 0;
}

