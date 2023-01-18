/**
 ******************************************************************************
 * @file    Smarthome_HT_Gateway_W\main\template\header_file.h
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Sep 25, 2017
 * @brief   
 * @history
 ******************************************************************************/


#ifndef __UART_VOICE_H
#define __UART_VOICE_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"
#include "driver/uart.h"


/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define UART_NUM_VOICE UART_NUM_2

#define DYNAMIC_PW_LEN  8

/* Exported types ------------------------------------------------------------*/
enum {
    WIFI_STATE_TO_MCU_DISCONNECT = 0,
    WIFI_STATE_TO_MCU_CONNECT,
    WIFI_STATE_TO_MCU_CONFIG,
    WIFI_STATE_TO_MCU_CONFIG_CONNECT
};

/* Exported functions ------------------------------------------------------- */
void UartVoice_Init();
void UartVoice_updateFirmVer();
void uartVoice_sendWifiState();
void uartVoice_sendTestEn();
void uartVoice_sendRaInfo();
void uartVoice_sendMeassage(char* str);
#endif /* __UART_VOICE_H */

