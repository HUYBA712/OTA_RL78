/**
 ******************************************************************************
 * @file    Smarthome_HT_Gateway_W\main\BLE_handler.h
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Sep 25, 2017
 * @brief   
 * @history
 ******************************************************************************/
/*
Note:
esp-idf have problem. when write long val to characteristic, some data can lost.
So in write event, save value. Do not read from char value.
*/

#ifndef __BLE_HANDLER_H
#define __BLE_HANDLER_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"
#include "HTG_Utility.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/


// message to sent to mobile
#define BLE_MES_WIFI_OK "Wifi_OK"

#define BLE_MES_USER_OK "User_OK"
#define BLE_MES_USER_EXIST "User_EXIST"
#define BLE_MES_USER_FALSE "User_FALSE"
#define BLE_MES_BLE_CONTROL_MODE_OK "BLE_OK"

/* Exported functions ------------------------------------------------------- */
void BLE_init();
void BLE_start(uint8_t* wifiList,uint16_t wifiListLen);
void BLE_sentWifiStateDone();
void BLE_StopBLE();
void BLE_sentToMobile(const char *sentMes);
void BLE_startBleControlMode();
void BLE_sendToBleControlChar(const char *sentMes);
void BLE_setPropertyData(const char* pcode,const char * pdata);
void BLE_reAdvertising();
void BLE_releaseBle();
/* Callback functions ------------------------------------------------------- */
// void haveWifiFromBLE(char *ssid, char *password);
void recievedBleDone();

void BLE_sendToGetWaterChar(const char *sentMes);

extern char g_new_ssid[32],  g_new_pwd[32];
extern bool g_haveNewSsid ;

#endif /* __BLE_HANDLER_H */
