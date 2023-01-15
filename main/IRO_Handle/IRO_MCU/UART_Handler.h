#ifndef UART_HANDLER_
#define UART_HANDLER_

#include "Global.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "cJSON.h"
static const int RX_BUF_SIZE = 512;

#if (HW_VER_NUMBER == 5)
#define RXD_PIN (GPIO_NUM_26)
#define TXD_PIN (GPIO_NUM_25)
#elif (HW_VER_NUMBER ==7)
#define RXD_PIN (GPIO_NUM_25)
#define TXD_PIN (GPIO_NUM_26)
#endif
#define MAX_FRAME_SEND_MCU 200

typedef enum{
	LED_BLINK_STATE_OFF = 		0,
	LED_BLINK_STATE_ON = 		1,
	LED_BLINK_STATE_BLINK = 	2
}LedBlinkState_t;

typedef struct 
{
	char buf[MAX_FRAME_SEND_MCU];
} bufSend_t;


void UART_Init(void);
void UART_SendCmd(char *command);
void UART_CreateReceiveTask();
void UART_ProcessCmd(uint8_t *data_rcv, uint16_t length);
void UART_UpdatePublishInfo(char *mid,char *reason, cJSON *info);
void UART_ClearPublishInfo(void);
void UART_needUpdateAllParam();
void UART_SendLedBlinkCmd(LedBlinkState_t blinkState,uint16_t interval);
void UART_sendToQueueToSend(char* buf);
void UART_SendLedBlinkCmdWithoutQueue(LedBlinkState_t blinkState,uint16_t interval);
void UART_requestInfoMcu();
void UART_SendVoiceCmd(uint16_t cmdIndex);
void UART_requestBslPassword();
void UART_sendRightNow(char* buf);
#endif