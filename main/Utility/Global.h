/**
 ******************************************************************************
 * @file    Smarthome_HT_Gateway_W\main\Global.h
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Sep 25, 2017
 * @brief   
 * @history
 ******************************************************************************/


#ifndef __GLOBAL_H
#define __GLOBAL_H

/* Includes ------------------------------------------------------------------*/
#include "esp_system.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define DEVICE_NAME            "TCM_waterPurifier"
#define VER_STRING "1.1.3"
#define FIRM_VER (1031)

#define HW_VER_MSP_VOICE      //
// #define HW_VER_MSP430      //
// #define HW_VER_3_1      //iro hw new: co dua chan ra ngoai
// #define HW_VER_3_0   //iro hw new: k dua chan ra ngoai
// #define HW_VER_2_1     //hw old: cam vao nhau, ng chan wd led voi Hw20
// #define HW_VER_2_0   //hw old: cam vao nhau
#ifdef HW_VER_MSP_VOICE
// #define HW_VER_NUMBER (5)       //d88 plus
#define HW_VER_NUMBER (7)       //aiotec5.0, 
//doi voi mcu thi msp hw7, rx130 hw17, 
//tuy nhien esp de chung 1 firmware hardware, phai tu check hard cua mcu de ota cho dung
#define LIS_MCU_HARD_RX130 {17}
#define LIS_MCU_HARD_MSP {7}


#elif HW_VER_MSP430
#define HW_VER_NUMBER (4)
#elif HW_VER_3_1
#define HW_VER_NUMBER (3)
#elif defined(HW_VER_3_0)
#define HW_VER_NUMBER (2)
#elif defined(HW_VER_2_1)
#define HW_VER_NUMBER (0)
#elif defined(HW_VER_2_0)
#define HW_VER_NUMBER (1)
#endif

/// define server
// #define SERVER_MAKIPOS   
#define SERVER_TECOMEN

#define HTTP_PWD "mkpsmarthome"
#define PASSWORD_LEN 20
#define PRODUCT_ID_LEN 30

typedef enum
{
    COM_MODE_WIFI,
    COM_MODE_BLE,
}ComMode_t;
#define NOT_REGISTER 0
#define HAD_REGISTER 1
extern ComMode_t g_comMode;


// API URL
#ifdef SERVER_MAKIPOS
#define URL_GET_TOCKEN  "http://server.makipos.net:3028/devices/authentication"
#define URL_RELATIONS   "http://server.makipos.net:3028/relations"
#define URL_SCHEDULE    "http://server.makipos.net:3028/devices-schedule"
#define URL_CHECK_UPDATE    "http://server.makipos.net:3028/update-device"
#define URL_PRE_OTA_DOWNLOAD_LINK "http://server.makipos.net:3028/update-device-static/"
#define URL_DEVICE_INFO "http://server.makipos.net:3028/devices"

#define MQTT_SERVER "server.makipos.net"
#define DEVICE_TYPE_STR "aiotec4_3_AWS_esp"
#define DEVICE_TYPE_MCU_STR "aiotec4_3_AWS_mcu"
#define DEVICE_TYPE_ID "5e55e7292790ea85f3e00d09"

#elif defined(SERVER_TECOMEN)

#define URL_GET_TOCKEN  "http://360server.karofi.com:3028/devices/authentication"
#define URL_RELATIONS   "http://360server.karofi.com:3028/relations"
#define URL_SCHEDULE    "http://360server.karofi.com:3028/devices-schedule"
#define URL_CHECK_UPDATE    "http://360server.karofi.com:3028/update-device"
#define URL_PRE_OTA_DOWNLOAD_LINK "http://360server.karofi.com:3028/update-device-static/"
#define URL_DEVICE_INFO "http://360server.karofi.com:3028/devices"

#define MQTT_SERVER "360server.karofi.com"
#define DEVICE_TYPE_STR "Aiotec_voice_lcd-AWS_esp"
#define DEVICE_TYPE_MCU_STR "Aiotec_voice_lcd-AWS_mcu"
#define DEVICE_TYPE_MCU_STR_VOICE "Aiotec_voice_lcd-AWS-mcu-voice"
#define DEVICE_TYPE_ID "6209c15fb69cbf967ad1ef0f"
#define DEVICE_TYPE_ID_AWS  22
#endif

#endif /* __GLOBAL_H */
