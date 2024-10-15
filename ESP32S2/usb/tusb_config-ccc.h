#ifndef MAIN_SRC_TUSB_CONFIG_H_
#define MAIN_SRC_TUSB_CONFIG_H_

#define CFG_TUSB_MCU            OPT_MCU_ESP32S2

#define BOARD_DEVICE_RHPORT_NUM 0
#define CFG_TUSB_OS             2

#define CFG_TUD_CDC             1

//------------- CDC -------------//

// FIFO size of CDC TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE   512
#define CFG_TUD_CDC_TX_BUFSIZE   512

//#define CFG_TUD_MSC             1

//#define CFG_TUD_MSC_EP_BUFSIZE

#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_DEVICE
#define CFG_TUSB_RHPORT1_MODE   OPT_MODE_NONE



#endif /* MAIN_SRC_TUSB_CONFIG_H_ */