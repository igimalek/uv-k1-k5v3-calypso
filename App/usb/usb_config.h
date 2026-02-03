 
#ifndef CHERRYUSB_CONFIG_H
#define CHERRYUSB_CONFIG_H


#define CONFIG_USB_PRINTF(...)  

#define usb_malloc(size) malloc(size)
#define usb_free(ptr)    free(ptr)

#ifndef CONFIG_USB_DBG_LEVEL
#define CONFIG_USB_DBG_LEVEL USB_DBG_ERROR
#endif

 
#define CONFIG_USB_PRINTF_COLOR_ENABLE

 
#ifndef CONFIG_USB_ALIGN_SIZE
#define CONFIG_USB_ALIGN_SIZE 4
#endif

 
#define USB_NOCACHE_RAM_SECTION __attribute__((section(".noncacheable")))


#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 256


#ifndef CONFIG_USBDEV_MSC_BLOCK_SIZE
#define CONFIG_USBDEV_MSC_BLOCK_SIZE 512
#endif

#ifndef CONFIG_USBDEV_MSC_MANUFACTURER_STRING
#define CONFIG_USBDEV_MSC_MANUFACTURER_STRING ""
#endif

#ifndef CONFIG_USBDEV_MSC_PRODUCT_STRING
#define CONFIG_USBDEV_MSC_PRODUCT_STRING ""
#endif

#ifndef CONFIG_USBDEV_MSC_VERSION_STRING
#define CONFIG_USBDEV_MSC_VERSION_STRING "0.01"
#endif


#ifdef CONFIG_USBDEV_MSC_THREAD
#ifndef CONFIG_USBDEV_MSC_STACKSIZE
#define CONFIG_USBDEV_MSC_STACKSIZE 2048
#endif

#ifndef CONFIG_USBDEV_MSC_PRIO
#define CONFIG_USBDEV_MSC_PRIO 4
#endif
#endif

#ifndef CONFIG_USBDEV_AUDIO_VERSION
#define CONFIG_USBDEV_AUDIO_VERSION 0x0100
#endif

#ifndef CONFIG_USBDEV_AUDIO_MAX_CHANNEL
#define CONFIG_USBDEV_AUDIO_MAX_CHANNEL 8
#endif


#include "py32f0xx.h"

#define USBD_IRQn       USB_IRQn

#define USBD_IRQHandler USB_IRQHandler

typedef struct
{
    uint8_t *buf;
    const uint32_t size;
    volatile uint32_t *write_pointer;
} cdc_acm_rx_buf_t;

void cdc_acm_init(cdc_acm_rx_buf_t rx_buf);
void cdc_acm_data_send_with_dtr(const uint8_t *buf, uint32_t size);
void cdc_acm_data_send_with_dtr_async(const uint8_t *buf, uint32_t size);

#endif
