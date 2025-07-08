/*
 * @file   main.c
 * @brief  USB CDC server.
 *  
 * The server uses a single input channel with two types of messages:
 * - interrupt message
 * - transfer request message
 * 
 * Interrupt messages are sent by the kernel for each hardware interrupt from
 * the device. Transfer request messages are sent by clients and contain a 
 * payload to be sent over USB.
 * If the device is busy when the tx request message arrives then the message
 * is preserved in the special private channel to be sent later.
 * 
 * In the current design an actor may own just a single message at any time.
 * So, the payload has to be copied into local buffer before sending as
 * there may be multiple accesses to the buffer in case when the payload does 
 * not fit into a single USB packet.
 */

#include <stddef.h>
#include <stdbool.h>
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "usbd_def.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "actinium.h"
#include "ipc.h"

/*
 * The USB stack uses callbacks that are called from the interrupt handling
 * routine. These globals are used for communication between those callbacks.
 */
static USBD_HandleTypeDef g_cdc_device;
static bool g_tx_complete = false;
static size_t g_rx_len = 0;
static size_t g_tx_msg_pending_count = 0;
static uint8_t g_rx_buffer[2048];
static uint8_t g_tx_buffer[2048];

#define MSG_PAYLOAD_SZ sizeof(((struct usb_msg_t*)0)->payload)

_Static_assert(
    sizeof(g_tx_buffer) >= MSG_PAYLOAD_SZ,
    "tx buffer must be larger than message payload");

void Error_Handler(void) {
    for (;;);
}

uint32_t HAL_RCC_GetHCLKFreq(void) {
    return 84000000u;
}

/*
 * FIXME: In the current release delay is only used once during enumeration
 * phase for 1ms. In order to make things simple just use busywait now.
 */
void HAL_Delay(uint32_t Delay) {
    const uint32_t duration = Delay * 50000u;

    for (uint32_t i = 0; i < duration; i++) {
        __NOP();
    }
}

/*
 * Freestanding compiler. No libc.
 */
void* memmove(void *dest, const void *src, size_t n) {
    const char *csrc = src;
    char *cdest = dest;

    for (size_t i = 0; i < n; i++) {
        cdest[i] = csrc[i];
    }

    return dest;
}

static int8_t CDC_Init_FS(void) {
    USBD_CDC_SetTxBuffer(&g_cdc_device, g_tx_buffer, 0);
    USBD_CDC_SetRxBuffer(&g_cdc_device, g_rx_buffer);
    return USBD_OK;
}

static int8_t CDC_DeInit_FS(void) {
    return USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length) {
    return USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t *length) {
    USBD_CDC_SetRxBuffer(&g_cdc_device, &pbuf[0]);
    USBD_CDC_ReceivePacket(&g_cdc_device);
    g_rx_len = *length;
    return USBD_OK;
}

static uint8_t CDC_Transmit_FS(uint8_t* pbuf, uint16_t length) {
    USBD_CDC_SetTxBuffer(&g_cdc_device, pbuf, length);
    return USBD_CDC_TransmitPacket(&g_cdc_device);
}

static int8_t CDC_TransmitCplt_FS(uint8_t* pbuf, uint32_t* length, uint8_t epnum) {
    g_tx_complete = true;
    return USBD_OK;
}

static USBD_CDC_ItfTypeDef USBD_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    CDC_TransmitCplt_FS
};

static inline void USB_Init(void)
{
    if (USBD_Init(&g_cdc_device, &FS_Desc, DEVICE_FS) != USBD_OK) {
        Error_Handler();
    }

    if (USBD_RegisterClass(&g_cdc_device, &USBD_CDC) != USBD_OK) {
        Error_Handler();
    }

    if (USBD_CDC_RegisterInterface(&g_cdc_device, &USBD_fops_FS) != USBD_OK) {
        Error_Handler();
    }

    if (USBD_Start(&g_cdc_device) != USBD_OK) {
        Error_Handler();
    }
}

static inline void Tx_Start(const struct usb_msg_t* msg) {
    const size_t len = MIN(msg->header.payload_len, MSG_PAYLOAD_SZ);
    memmove(g_tx_buffer, msg->payload, len);
    CDC_Transmit_FS(g_tx_buffer, len);
}

static inline void Tx_Msg_Process(const struct usb_msg_t* msg) {
    if (++g_tx_msg_pending_count == 1) {
        Tx_Start(msg);
    } else {
        ac_push(CHAN_USB_SERVER_PRIV);
    }
}

static inline void Tx_Completion_Check(void) {
    if (g_tx_complete) {
        g_tx_complete = false;

        if (--g_tx_msg_pending_count) {
            struct usb_msg_t* msg = ac_try_pop(CHAN_USB_SERVER_PRIV);
            if (msg) {
                Tx_Start(msg);
            }
        }
    }
}

static inline void Rx_Check(struct usb_msg_t* msg) {
    if (g_rx_len) {
        const size_t len = MIN(g_rx_len, sizeof(msg->payload));
        msg->header.payload_len = len;
        memmove(msg->payload, g_rx_buffer, len);
        g_rx_len = 0;
        ac_push(CHAN_USB_SERVER_OUT);
    }
}

static inline void Incoming_Msg_Process(struct usb_msg_t* msg) {
    extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

    switch (msg->header.opcode) {
    case USB_INTERRUPT:        
        HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
        USB_EnableGlobalInt(USB_OTG_FS);
        Rx_Check(msg);
        Tx_Completion_Check();
        break;
    case USB_TX_REQUEST:
        Tx_Msg_Process(msg);
        break;
    default:
        break;
    }
}

uint32_t main(struct usb_msg_t* msg) {
    if (msg == NULL) {
        USB_Init();
    } else {
        Incoming_Msg_Process(msg);
    }
    
    return ac_subscribe_to(CHAN_USB_SERVER_IN);
}

