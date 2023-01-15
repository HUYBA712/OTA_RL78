/**
 ******************************************************************************
 * @file    Smarthome_HT_Gateway_W\main\Wifi_handler.h
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Sep 25, 2017
 * @brief
 * @history
 ******************************************************************************/


#ifndef __WIFI_HANDLER_H
#define __WIFI_HANDLER_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"
/* Exported types ------------------------------------------------------------*/
typedef enum
{
	Wifi_State_None,
	Wifi_State_Started,
	Wifi_State_Got_IP,
} Wifi_State;

// WIFI connected bit
// this bit is clear when 
// 			1, wifi is not conected 
// 			2, In wifi config mode 
#define CONNECTED_BIT BIT0
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define WIFI_LIST_MAX_LEN 500
//
/* Exported functions ------------------------------------------------------- */
void Wifi_init();
void Wifi_start();
void Wifi_startConnect(char* ssid, char*password);
Wifi_State getWifiState();
void Wifi_reConnect();

/* Callback functions ------------------------------------------------------- */
void wifiConnectDone();
void Wifi_checkRssi();
void Wifi_startScan();
void Wifi_startConfigMode();
void Wifi_getMacStr();
void Wifi_getInfoStaCfg();
void setProductId_defaultMac();
void Wifi_autoCfg();
void Wifi_retryToConnect();
/* Extern variable ------------------------------------------------------- */
extern EventGroupHandle_t wifi_event_group;
extern uint8_t Wifi_ListSSID[WIFI_LIST_MAX_LEN] ;
extern uint16_t Wifi_ListSsidLen;
extern bool g_haveWifiList;
extern bool g_wifiNeedReconnect ;

#define WIFI_HANDLER_IS_CONECTED  (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT)
#define WIFI_HANDLER_WAIT_CONECTED_NOMAL_FOREVER while((xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,false, true, portMAX_DELAY)& CONNECTED_BIT)  == 0)

#endif /* __WIFI_HANDLER_H */
