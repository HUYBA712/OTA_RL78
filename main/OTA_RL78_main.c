#include "Gateway_config.h"
#include "nvs_flash.h"
#include "Global.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "Wifi_handler.h"
#include "HttpHandler.h"
#include "MqttHandler.h"
#include "HTG_Utility.h"
#include "OutputControl.h"
#include "BLE_handler.h"
#include "DateTime.h"
#include "esp32/rom/rtc.h"
#include "WatchDog.h"
#include "UART_Handler.h"
#include "OTA_http.h"
#include "FlashHandle.h"
#include "BLD_mcu.h"
#include "UartVoice.h"
#include "BLD_mcuVoice.h"

#define TAG "Main App"
extern EventGroupHandle_t wifi_event_group;
uint8_t g_registerStatus = NOT_REGISTER;  //not register
char g_macDevice[6];
char g_product_Id[PRODUCT_ID_LEN];
char g_password[PASSWORD_LEN] = HTTP_PWD;
void printResetReason()
{
    // ESP_LOGI(TAG, "FW ver: 1.1.1 --------------------------------------------------------------- \n");
    ESP_LOGI(TAG, "reset reason is: [%d] \n", rtc_get_reset_reason(0));
    ESP_LOGI(TAG, "Firmware ver: %d \n", FIRM_VER);
}
void app_main()
{
    // EWD_init();
    // nvs_flash_init();
    // printResetReason();
    // UART_Init();
    // Wifi_init();
    // setProductId_defaultMac();
    
    // sprintf(g_product_Id, "%s", "43A00000030");
    // sprintf(g_password, "%s", "mkpsmarthome");
    // FlashHandle_initInfoPwd();
    // bldMcu_initPw();
    // MQTT_Init();
    // bldMcuVoice_pinInit();
    // bldMcuVoice_bootModeEntry();
    // UART_CreateReceiveTask(); //start task receive UART
    // Out_LedIndicate();
    // GatewayConfig_autoCfg();
    UartVoice_Init();
    // // initDateTime();  
    bldMcuVoice_taskUpdate();
    while (1)
    {
        printf("Free heap: %d bytes\n", xPortGetFreeHeapSize());
        vTaskDelay(5000/ portTICK_RATE_MS);
    }
    // end test code-------------------------------------------------

    return;
}

