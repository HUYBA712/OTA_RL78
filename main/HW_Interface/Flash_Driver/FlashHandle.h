/**
 ******************************************************************************
 * @file    FlashHandler.h
 * @author  Makipos Co.,LTD.
 * @date    Nov 29, 2019
 ******************************************************************************/


#ifndef __FLASH_HANDLER_H
#define __FLASH_HANDLER_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void FlashHandler_init();
bool FlashHandler_getData(char* nameSpace,char* key,void* dataStore);
bool FlashHandler_getDeviceInfoInStore();
bool FlashHandler_saveDeviceInfoInStore();
bool FlashHandler_getComMode();
bool FlashHandler_saveComMode();
void FlashHandle_initInfoPwd();
bool FlashHandler_getRegisterStt();
bool FlashHandler_saveRegisterStt();
bool FlashHandler_getMcuOtaStt();
bool FlashHandler_saveMcuOtaStt();
bool FlashHandler_saveMcuPassword();
bool FlashHandler_getMcuPassword();
bool FlashHandler_getCertKeyMqttInStore();
bool FlashHandler_saveCertKeyMqttInStore();
bool FlashHandler_saveEnvironmentMqttInStore();
bool FlashHandler_getEnvironmentMqttInStore();
#endif /* __FLASH_HANDLER_H */
