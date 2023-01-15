/**
 ******************************************************************************
 * @file    Smarthome_HT_Gateway_W\main\gateway_config.c
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Sep 25, 2017
 * @brief
 * @history
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "Gateway_config.h"
#include "BLE_handler.h"
#include "Wifi_handler.h"
#include "HttpHandler.h"
#include "OutputControl.h"
#include "FlashHandle.h"
#include "esp32/rom/rtc.h"
#include "UART_Handler.h"
#include "OTA_http.h"
#include "MqttHandler.h"
#include "UartVoice.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define GATEWAY_CONFIG_TAG "GATEWAY_CONFIG"
#define TOUCH_PRESS_TIMEOUT 15 //second
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
//
/*******************************************************************************
 * External Variables
 ******************************************************************************/
extern bool g_bleConfigConnected;
extern bool g_enTest;
extern bool needSendFirmVer;
/*******************************************************************************
* Variables
******************************************************************************/
bool isWifiCofg;
bool g_newInfoWifiPass = false;
/*******************************************************************************
 * BLE handler Call back
 ******************************************************************************/
void haveWifiFromBLE(char *ssid, char *password)
{
	g_newInfoWifiPass = true;
	ESP_LOGI(GATEWAY_CONFIG_TAG, "Have wifi, ssid: %s, password %s\n", ssid, password);
	Wifi_startConnect(ssid, password);
}

void haveUserInfo(const char *userID)
{
	ESP_LOGI(GATEWAY_CONFIG_TAG, "New User ID: %s\n", userID);
	char *userStr = (char *)calloc(50, 1);
	memcpy(userStr, userID, strlen(userID));
	Http_addDeviceToUser(userStr);
}
void addDeviceToUserDone(int result)
{
	if (result < 0)
	{
		ESP_LOGE(GATEWAY_CONFIG_TAG, "Can't add device to user\n");
		BLE_sentToMobile(BLE_MES_USER_FALSE);
	}
	else if (result == 200)
	{
		ESP_LOGI(GATEWAY_CONFIG_TAG, "add this device to user success\n");
		BLE_sentToMobile(BLE_MES_USER_OK);
	}
	else if (result == 409)
	{
		ESP_LOGI(GATEWAY_CONFIG_TAG, "user alredy have this device\n");
		BLE_sentToMobile(BLE_MES_USER_EXIST);
	}
	vTaskDelay(10000 / portTICK_RATE_MS);
	isWifiCofg = false;
	esp_restart(); // for test ---------------------------------------------
}

void recievedBleDone()
{
	esp_restart();
}

/*******************************************************************************
 * Wifi handler Call back
 ******************************************************************************/
extern bool state_bleWifi;
extern bool g_enTest;
extern bool needSendFirmVerMqtt; 
extern bool g_isMqttConnected;
extern uint16_t mcuFirmVer;
extern uint16_t mcuHardVer;
void wifiConnectDone()
{
	if (isWifiCofg)
	{
		printf("wifi connect done\n");
		if(g_newInfoWifiPass)
		{
			state_bleWifi = true;
			BLE_sentToMobile(BLE_MES_WIFI_OK);
		}
	}
}
/************************************************
 * Application functions
 ************************************************/
extern bool touchLongRelease;
static void gatewayConfig_task(void *pvParameter)
{
	GatewayConfig_start();
	vTaskDelete(NULL);
}

void GatewayConfig_init()
{
	if (isWifiCofg)
	{
		BLE_reAdvertising();
		ESP_LOGW(GATEWAY_CONFIG_TAG, "Config is alredy runing");
		return;
	}
	xTaskCreate(gatewayConfig_task, "gatewayConfig_task", 4096, NULL, 8, NULL);
}
extern bool s_bleControlConnected;
void GatewayConfig_start()
{
	isWifiCofg = true;
	// xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
	Wifi_startConfigMode();
	g_newInfoWifiPass = false;
	UART_SendLedBlinkCmd(LED_BLINK_STATE_BLINK,200);
	uartVoice_sendWifiState();
	// Out_setAllLed(1);
	uint16_t timeNotConnect = 0;
	uint16_t timeConnect = 0;
	while ((timeNotConnect < 1000) && (timeConnect < 180) && isWifiCofg)
	{
		if(needSendFirmVer)
        {
            Http_sendFirmWareVer();
            needSendFirmVer = false;
        }
		if(needSendFirmVerMqtt && g_isMqttConnected)
        {
            MQTT_PublishVersion(HW_VER_NUMBER, FIRM_VER, mcuFirmVer, mcuHardVer);
            needSendFirmVerMqtt = false;
        }
		if (g_wifiNeedReconnect)
		{
			Wifi_reConnect();
			g_wifiNeedReconnect = false;
		}
		if (g_haveWifiList)
		{
			BLE_start(Wifi_ListSSID, Wifi_ListSsidLen);
			g_haveWifiList = false;
		}
		if (g_haveNewSsid)
		{
			g_haveNewSsid = false;
			haveWifiFromBLE(g_new_ssid, g_new_pwd);
		}
		if(g_enTest)
		{
			vTaskDelay(100/portTICK_PERIOD_MS);
			continue;
		}

		if (g_bleConfigConnected || s_bleControlConnected)
		{
			Out_setAllLed(timeConnect % 2);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			timeConnect++;
		}
		else
		{
			// if (getWifiState() != Wifi_State_Got_IP)
			// {
				Out_setAllLed(timeNotConnect % 2);
			// }
			vTaskDelay(100 / portTICK_PERIOD_MS);
			timeNotConnect++;
		}
	}
	ESP_LOGW(GATEWAY_CONFIG_TAG, "Config is timeout: time connect %d not %d ",timeConnect,timeNotConnect);
	// BLE_StopBLE();
	if (!FlashHandler_getComMode())
	{
		FlashHandler_saveComMode();
	}
	// if (g_comMode == COM_MODE_WIFI)
	// {
	// 	esp_restart();
	// 	isWifiCofg = false;
	// }
	// else if (g_comMode == COM_MODE_BLE)
	// {
	// 	BLE_startBleControlMode();
	// 	isWifiCofg = false;
	// 	UART_SendLedBlinkCmd(LED_BLINK_STATE_OFF,0);
	// }
	isWifiCofg = false;
	BLE_startBleControlMode();
	Wifi_retryToConnect();
	if (getWifiState() != Wifi_State_Got_IP)
	{
		UART_SendLedBlinkCmd(LED_BLINK_STATE_OFF,0);
	}
	else{
		UART_SendLedBlinkCmd(LED_BLINK_STATE_ON,0);
	}
	uartVoice_sendWifiState();
	OTA_http_checkAndDo(); 
}

void GatewayConfig_autoCfg()
{
	if (!FlashHandler_getComMode())
	{
		FlashHandler_saveComMode();
	}

	BLE_startBleControlMode();
	UART_SendLedBlinkCmd(LED_BLINK_STATE_OFF,0);
	if (rtc_get_reset_reason(0) == POWERON_RESET || rtc_get_reset_reason(0) == RTCWDT_RTC_RESET)
	{
		ESP_LOGW(GATEWAY_CONFIG_TAG, "power up");
		// vTaskDelay(3000 / portTICK_RATE_MS);
		GatewayConfig_init();
	}
	else
	{
		Wifi_start();
		OTA_http_checkAndDo(); 	
	}
}

/***********************************************/
