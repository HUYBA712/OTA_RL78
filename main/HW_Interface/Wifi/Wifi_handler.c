/**
 ******************************************************************************
 * @file    Smarthome_HT_Gateway_W\main\Wifi_handler.c
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Sep 25, 2017
 * @brief
 * @history
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "Wifi_handler.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "Gateway_config.h"
#include "MqttHandler.h"
#include "HTG_Utility.h"
#include "OutputControl.h"
#include "UART_Handler.h"
#include "WatchDog.h"
#include "timeCheck.h"
#include "esp_netif.h"
#include "UartVoice.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#ifndef DISABLE_LOG_I
#define WIFI_HANDLER_LOG_INFO_ON
#endif

#ifdef WIFI_HANDLER_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI("Wifi_handler", format, ##__VA_ARGS__)
#define log_err(format, ...) ESP_LOGE("Wifi_handler", format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#endif

#define SSID_SEPARATE 0x06
#define WIFI_HOST_NAME_MKH_SWITCH "Tecomen_aiotec"
#define MAX_TIME_NOT_CONNECTED 300000                // tick
#define WIFI_RECONNECT_INTERVAL 30000
/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
* Variables
******************************************************************************/
Wifi_State wifiState = Wifi_State_None;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;
ip4_addr_t myLocalIp;
uint8_t Wifi_ListSSID[WIFI_LIST_MAX_LEN];
uint16_t Wifi_ListSsidLen = 0;
extern char g_macDevice[6];
extern char g_product_Id[PRODUCT_ID_LEN];
bool g_haveWifiList = false;
bool g_wifiNeedReconnect = false;
/*******************************************************************************
 * Code
 ******************************************************************************/
extern bool wifiConnectedFirstTimes;
bool disableReconnect = false;
void Wifi_disableReconnect()
{
    disableReconnect = true;
}

/************************************************
 * Wifi event
 ************************************************/
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)

{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE)
    {
        uint16_t apCount = 0;
        esp_wifi_scan_get_ap_num(&apCount);
        if (apCount == 0)
        {
            Wifi_startScan();
            return;
        }
        wifi_ap_record_t *list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, list));

        int i;
        Wifi_ListSsidLen = 0;
        log_info("======================================================================\n");
        log_info("             SSID             |    RSSI    |           AUTH           |       MAC       \n");
        log_info("======================================================================\n");
        for (i = 0; i < apCount; i++)
        {
            char *authmode;
            switch (list[i].authmode)
            {
            case WIFI_AUTH_OPEN:
                authmode = "WIFI_AUTH_OPEN";
                break;
            case WIFI_AUTH_WEP:
                authmode = "WIFI_AUTH_WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                authmode = "WIFI_AUTH_WPA_PSK";
                break;
            case WIFI_AUTH_WPA2_PSK:
                authmode = "WIFI_AUTH_WPA2_PSK";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                authmode = "WIFI_AUTH_WPA_WPA2_PSK";
                break;
            default:
                authmode = "Unknown";
                break;
            }
            log_info("%26.26s    |    % 4d    |    %22.22s|%x:%x:%x:%x:%x:%x\n", list[i].ssid, list[i].rssi, authmode,list[i].bssid[0],list[i].bssid[1],list[i].bssid[2],list[i].bssid[3],list[i].bssid[4],list[i].bssid[5]);
            Buffer_add(Wifi_ListSSID,&Wifi_ListSsidLen,list[i].ssid,strlen((char*)list[i].ssid));
            if (i != (apCount - 1))
            {
                uint8_t tmp = SSID_SEPARATE;
                Buffer_add(Wifi_ListSSID,&Wifi_ListSsidLen,&tmp,1);
            }
                
        }

        g_haveWifiList = true;
        free(list);
        log_info("\n\n");
        if(wifiState != Wifi_State_Got_IP)
        {
            esp_wifi_connect();
        }
    }
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        log_info( "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        wifiState = Wifi_State_Got_IP;
        wifiConnectedFirstTimes = true;
        log_info("We have now connected to a station and can do things...\n");
        wifiConnectDone(); // call back
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        MQTT_Start();
        uartVoice_sendWifiState();
        if(!isWifiCofg){
            UART_SendLedBlinkCmd(LED_BLINK_STATE_ON,0);
        }
    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        log_info("SYSTEM_EVENT_STA_CONNECTED...\n");
    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        log_info("SYSTEM_EVENT_STA_DISCONNECTED...\n");
        wifiState = Wifi_State_Started;
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        //MQTT_Stop();
        if(isWifiCofg)
        {
            Wifi_retryToConnect();
        }

        uartVoice_sendWifiState();
        if (!isWifiCofg)
        {
            UART_SendLedBlinkCmd(LED_BLINK_STATE_OFF,0);
        }
    }

    return;
}

/************************************************
 * Application functions
 ************************************************/
void Wifi_retryToConnect()
{
    log_info("retry_wifi...");
    wifi_config_t getcfg;
    esp_wifi_get_config(ESP_IF_WIFI_STA, &getcfg);
    log_info("SSID: %s, PW: %s", getcfg.sta.ssid, getcfg.sta.password);
    if(getcfg.sta.ssid[0] == '\0') return;
    Wifi_startConnect((char *)getcfg.sta.ssid, (char *)getcfg.sta.password);
}

static void Wifi_watchWifiTask(void *pram)
{
    uint32_t timeWifiDisconected = 0;
    uint32_t timeReconnectWifi = 0;
    while (1)
    {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        if (getWifiState() != Wifi_State_Got_IP)
        {
            // thời điểm mất kết nối
            if (timeWifiDisconected == 0)
            {
                timeWifiDisconected = xTaskGetTickCount();
                timeReconnectWifi = timeWifiDisconected;
            }
            else
            {
                // sau 1 khoảng thời gian, reconect wifi
                if ((!isWifiCofg) && (elapsedTime(xTaskGetTickCount(), timeReconnectWifi) > WIFI_RECONNECT_INTERVAL))
                {
                    // Wifi_retryToConnect();
                    esp_wifi_connect();
                    timeReconnectWifi = xTaskGetTickCount();
                }
                // restart module nếu mất kết nối quá lâu
                if ((!isWifiCofg) && (elapsedTime(xTaskGetTickCount(), timeWifiDisconected) > MAX_TIME_NOT_CONNECTED))
                {
                    // EWD_hardRestart();    //tam thoi k reset
                }
            }
        }
        else
        {
            timeWifiDisconected = 0;
        }
    }
}
Wifi_State getWifiState() {
    return wifiState;
}

void Wifi_getMacStr()
{
    uint8_t l_Mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, l_Mac);
    memcpy(g_macDevice, l_Mac, sizeof(l_Mac));
    printf("MAC: %02x%02x%02x%02x%02x%02x\n",l_Mac[0], l_Mac[1], l_Mac[2], l_Mac[3], l_Mac[4], l_Mac[5]);

}
void setProductId_defaultMac()
{
    sprintf(g_product_Id, "%02x%02x%02x%02x%02x%02x", g_macDevice[0], g_macDevice[1], g_macDevice[2], g_macDevice[3], g_macDevice[4], g_macDevice[5]);
    printf("ProductId_default: %s\n",g_product_Id);
}

void Wifi_init(){
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); 
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));  
    Wifi_getMacStr(); 
}
void Wifi_start() {
    if (wifiState == Wifi_State_None)
    {
        wifiState = Wifi_State_Started;
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_start() );

        Wifi_getInfoStaCfg();
        xTaskCreate(&Wifi_watchWifiTask, "Wifi_watchWifiTask", 3*1024, NULL, 1, NULL);
    }
}

void Wifi_startScan() {
    // Let us test a WiFi scan ...
    wifi_scan_config_t scanConf = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true};
    for (uint8_t i = 0; i < 5; i++)
    {
        if (ESP_OK == esp_wifi_scan_start(&scanConf, false)) //The true parameter cause the function to block until
        {
            break;
        }
        else
        {
            log_err("can not scan wifi");   
        }
    }
}

void Wifi_startConnect(char* ssid, char*password) {
    if(wifiState == Wifi_State_Got_IP && !isWifiCofg) //timeout config 
    {
        disableReconnect = false;
        printf("Wifi got IP exist!!!\n");
        return;
    }
    if(wifiState == Wifi_State_Got_IP)  //if connected, disconnected to connect with new ssid
    {
        esp_wifi_disconnect();
    }
    disableReconnect = false;
    wifi_config_t wifi_config = {
        .sta = {
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);
    if(isWifiCofg)    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void Wifi_reConnect() {
    esp_wifi_disconnect();
    esp_wifi_connect();
}
void Wifi_startConfigMode() {
    if(wifiState == Wifi_State_Got_IP)
    {
        Wifi_startScan();
        return;
    }
    else{
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        Wifi_disableReconnect();
        Wifi_start();
        esp_wifi_disconnect();
        wifiState = Wifi_State_Started;
        Wifi_startScan();
    }
}

void Wifi_getInfoStaCfg()
{
    wifi_config_t getcfg;  
    esp_wifi_get_config(ESP_IF_WIFI_STA, &getcfg);
    log_info("MAC_AP:"MACSTR", SSID: %s, PW: %s",MAC2STR(getcfg.sta.bssid),getcfg.sta.ssid,getcfg.sta.password);
}

void Wifi_autoCfg()
{
    // wifi_config_t getcfg;  
    // esp_wifi_get_config(ESP_IF_WIFI_STA, &getcfg);
    // log_info("MAC_AP:"MACSTR", SSID: %s, PW: %s",MAC2STR(getcfg.sta.bssid),getcfg.sta.ssid,getcfg.sta.password);

    // if(rtc_get_reset_reason(0) == POWERON_RESET || rtc_get_reset_reason(0) == RTCWDT_RTC_RESET)
    // {
    //     log_info("power up");
    //     vTaskDelay(3000/ portTICK_RATE_MS);
    //     GatewayConfig_init();
    // }else if (g_comMode == COM_MODE_BLE)
    // {
    //     //ble mode
    //     BLE_startBleControlMode();
    // }
    
}
void Wifi_checkRssi()
{
    wifi_ap_record_t wifidata;
    if (esp_wifi_sta_get_ap_info(&wifidata)==0){
        printf("rssi:%d\r\n", wifidata.rssi);
        MQTT_PublishRssi(wifidata.rssi);
    }
}
/***********************************************/
