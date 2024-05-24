/**
  ******************************************************************************
  * @file      startup_stm32f401xc.s
  * @author    MCD Application Team
  * @brief     STM32F401xCxx Devices vector table for GCC based toolchains. 
  *            This module performs:
  *                - Set the initial SP
  *                - Set the initial PC == Reset_Handler,
  *                - Set the vector table entries with the exceptions ISR address
  *                - Branches to main in the C library (which eventually
  *                  calls main()).
  *            After Reset the Cortex-M4 processor is in Thread mode,
  *            priority is Privileged, and the Stack is set to Main.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
    
  .syntax unified
  .cpu cortex-m4
  .fpu softvfp
  .thumb

.global  g_pfnVectors

/* start address for the initialization values of the .data section. 
defined in linker script */
.word  _sidata
/* start address for the .data section. defined in linker script */  
.word  _sdata
/* end address for the .data section. defined in linker script */
.word  _edata
/* start address for the .bss section. defined in linker script */
.word  _sbss
/* end address for the .bss section. defined in linker script */
.word  _ebss
/* stack used for SystemInit_ExtMemCtl; always internal RAM used */

/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event. Only the absolutely
 *          necessary set is performed, after which the application
 *          supplied main() routine is called. 
 * @param  None
 * @retval : None
*/

.section  .text.Reset_Handler
.weak  Reset_Handler
.type  Reset_Handler, %function
Reset_Handler:  

/* Copy the data segment initializers from flash to SRAM */  
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b LoopCopyDataInit

CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit
  
/* Zero fill the bss segment. */
  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str  r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss

/* Call the application's entry point.*/
  b  main
.size  Reset_Handler, .-Reset_Handler

.section  .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b  Infinite_Loop
  .size  Default_Handler, .-Default_Handler

/******************************************************************************
*
* The minimal vector table for a Cortex M3. Note that the proper constructs
* must be placed on this to ensure that it ends up at physical address
* 0x0000.0000.
* 
*******************************************************************************/
.section  .isr_vector,"a",%progbits
.type  g_pfnVectors, %object
    
g_pfnVectors:
  .word  _estack
  .word  Reset_Handler
  .word  NMI_Handler
  .word  HardFault_Handler
  .word  MemManage_Handler
  .word  BusFault_Handler
  .word  UsageFault_Handler
  .word  0
  .word  0
  .word  0
  .word  0
  .word  ac_svc_entry
  .word  DebugMon_Handler
  .word  0
  .word  PendSV_Handler
  .word  SysTick_Handler
  
  /* External Interrupts */
  .word     ac_intr_entry           /* Window WatchDog  */                                        
  .word     ac_intr_entry           /* PVD through EXTI Line detection */                        
  .word     ac_intr_entry           /* Tamper and TimeStamps through the EXTI line */            
  .word     ac_intr_entry           /* RTC Wakeup through the EXTI line */                      
  .word     ac_intr_entry           /* FLASH */                                          
  .word     ac_intr_entry           /* RCC                          */                                            
  .word     ac_intr_entry           /* EXTI Line0                   */                        
  .word     ac_intr_entry           /* EXTI Line1                   */                          
  .word     ac_intr_entry           /* EXTI Line2                   */                          
  .word     ac_intr_entry           /* EXTI Line3                   */                          
  .word     ac_intr_entry           /* EXTI Line4                   */                          
  .word     ac_intr_entry           /* DMA1 Stream 0                */                  
  .word     ac_intr_entry           /* DMA1 Stream 1                */                   
  .word     ac_intr_entry           /* DMA1 Stream 2                */                   
  .word     ac_intr_entry           /* DMA1 Stream 3                */                   
  .word     ac_intr_entry           /* DMA1 Stream 4                */                   
  .word     ac_intr_entry           /* DMA1 Stream 5                */                   
  .word     ac_intr_entry           /* DMA1 Stream 6                */                   
  .word     ac_intr_entry           /* ADC1                         */                   
  .word     0               	 /* Reserved                     */
  .word     0              		 /* Reserved                     */                          
  .word     0                    /* Reserved                     */                          
  .word     0                    /* Reserved                     */                          
  .word     ac_intr_entry           /* External Line[9:5]s          */                          
  .word     ac_intr_entry           /* TIM1 Break and TIM9          */         
  .word     ac_intr_entry           /* TIM1 Update and TIM10        */         
  .word     ac_intr_entry           /* TIM1 Trigger and Commutation and TIM11 */
  .word     ac_intr_entry           /* TIM1 Capture Compare         */                          
  .word     ac_intr_entry           /* TIM2                         */                   
  .word     ac_intr_entry           /* TIM3                         */                   
  .word     ac_intr_entry           /* TIM4                         */                   
  .word     ac_intr_entry           /* I2C1 Event                   */                          
  .word     ac_intr_entry           /* I2C1 Error                   */                          
  .word     ac_intr_entry           /* I2C2 Event                   */                          
  .word     ac_intr_entry           /* I2C2 Error                   */                            
  .word     ac_intr_entry           /* SPI1                         */                   
  .word     ac_intr_entry           /* SPI2                         */                   
  .word     ac_intr_entry           /* USART1                       */                   
  .word     ac_intr_entry           /* USART2                       */                   
  .word     0               	 /* Reserved                     */                   
  .word     ac_intr_entry           /* External Line[15:10]s        */                          
  .word     ac_intr_entry           /* RTC Alarm (A and B) through EXTI Line */                 
  .word     ac_intr_entry           /* USB OTG FS Wakeup through EXTI line */                       
  .word     0                    /* Reserved     				 */         
  .word     0                    /* Reserved       			     */         
  .word     0                    /* Reserved 					 */
  .word     0                    /* Reserved                     */                          
  .word     ac_intr_entry           /* DMA1 Stream7                 */                          
  .word     0                    /* Reserved                     */                   
  .word     ac_intr_entry           /* SDIO                         */                   
  .word     ac_intr_entry           /* TIM5                         */                   
  .word     ac_intr_entry           /* SPI3                         */                   
  .word     0                    /* Reserved                     */                   
  .word     0                    /* Reserved                     */                   
  .word     0                    /* Reserved                     */                   
  .word     0                    /* Reserved                     */
  .word     ac_intr_entry           /* DMA2 Stream 0                */                   
  .word     ac_intr_entry           /* DMA2 Stream 1                */                   
  .word     ac_intr_entry           /* DMA2 Stream 2                */                   
  .word     ac_intr_entry           /* DMA2 Stream 3                */                   
  .word     ac_intr_entry           /* DMA2 Stream 4                */                   
  .word     0                    /* Reserved                     */                   
  .word     0              		 /* Reserved                     */                     
  .word     0              		 /* Reserved                     */                          
  .word     0             		 /* Reserved                     */                          
  .word     0              		 /* Reserved                     */                          
  .word     0              		 /* Reserved                     */                          
  .word     ac_intr_entry           /* USB OTG FS                   */                   
  .word     ac_intr_entry           /* DMA2 Stream 5                */                   
  .word     ac_intr_entry           /* DMA2 Stream 6                */                   
  .word     ac_intr_entry           /* DMA2 Stream 7                */                   
  .word     ac_intr_entry           /* USART6                       */                    
  .word     ac_intr_entry           /* I2C3 event                   */                          
  .word     ac_intr_entry           /* I2C3 error                   */                          
  .word     0                    /* Reserved                     */                   
  .word     0                    /* Reserved                     */                   
  .word     0                    /* Reserved                     */                         
  .word     0                    /* Reserved                     */                   
  .word     0                    /* Reserved                     */                   
  .word     0                    /* Reserved                     */                   
  .word     0                    /* Reserved                     */
  .word     ac_intr_entry           /* FPU                          */
  .word     0                    /* Reserved                     */                   
  .word     0                    /* Reserved                     */
  .word     ac_intr_entry           /* SPI4                         */     
                    
  .size  g_pfnVectors, .-g_pfnVectors

.weak      NMI_Handler
.thumb_set NMI_Handler,Default_Handler

.weak      HardFault_Handler
.thumb_set HardFault_Handler,Default_Handler

.weak      MemManage_Handler
.thumb_set MemManage_Handler,Default_Handler

.weak      BusFault_Handler
.thumb_set BusFault_Handler,Default_Handler

.weak      UsageFault_Handler
.thumb_set UsageFault_Handler,Default_Handler

.weak      SVC_Handler
.thumb_set SVC_Handler,Default_Handler

.weak      DebugMon_Handler
.thumb_set DebugMon_Handler,Default_Handler

.weak      PendSV_Handler
.thumb_set PendSV_Handler,Default_Handler

