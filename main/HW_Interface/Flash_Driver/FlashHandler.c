/**
 ******************************************************************************
 * @file    AutoRule.c
 * @author  Makipos Co.,LTD.
 * @date    June 19, 2019
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "FlashHandle.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/queue.h"
#include "HTG_Utility.h"
#include "BLD_mcu.h"
#include "MqttHandler.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#ifndef DISABLE_LOG_I
#define FLASH_HANDLER_LOG_INFO_ON
#endif

#ifdef FLASH_HANDLER_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI("FLASH_HANDLER", format, ##__VA_ARGS__)
#define log_err(format, ...) ESP_LOGE("FLASH_HANDLER", format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#define log_err(format, ...)
#endif

#define NAMESPACE_GENARAL "DEVICE_GENARAL"
#define KEY_DEVICE_INFO_ID_PWS "InfoIdPassword"
#define KEY_COM_MODE        "ComMode"
#define KEY_REGISTER_STATUS "RegisterStt"
#define KEY_FIRM_MCU_STATUS "newFirmMcu"
#define KEY_PW_MCU "pwMcu"
#define KEY_CERTKEY_MQTT       "mqttCertkey"
#define KEY_ENVIR_MQTT       "mqttEnvir"
typedef struct DataInfoIdStr
{
    char productId[PRODUCT_ID_LEN];
    char password[PASSWORD_LEN];
    // bool isCreated;
}DataIdInfo_t;

/*******************************************************************************
 * External Variables
 ******************************************************************************/
extern char g_password[PASSWORD_LEN];
extern char g_product_Id[PRODUCT_ID_LEN]; 
/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/


/*******************************************************************************
 * Local Function
 ******************************************************************************/



/*******************************************************************************
 * Public Function
 ******************************************************************************/
bool FlashHandler_getData(char* nameSpace,char* key,void* dataStore)
{
    log_info("Getting data store in flash");
    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(nameSpace, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        log_err("open nvs error");
        return false;
    }

    // Read the size of memory space required for blob
    size_t required_size = 0; // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, key, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        log_err("nvs_get_blob error");
        return false;
    }

    // Read previously saved blob if available
    if (required_size > 0)
    {
        log_info("Found data store in flash");
        err = nvs_get_blob(my_handle, key, dataStore, &required_size);
        if (err != ESP_OK)
        {
            log_err("nvs_get_blob error");
            return false;
        }
    }

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        log_err("nvs_commit error");
        return false;
    }
    // Close
    nvs_close(my_handle);
    log_info("Get data store in flash done");
    if (required_size >0)
    {
        return true;
    }
    
    return false;
}
bool FlashHandler_setData(char* nameSpace,char* key,void* dataStore,size_t dataSize)
{
    log_info("Saving data to flash");
    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(nameSpace, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        log_err("open nvs error");
        return false;
    }

    // Read the size of memory space required for blob
    size_t required_size = 0; // value will default to 0, if not set yet in NVS
    // Write value including previously saved blob if available
    required_size = dataSize;
    log_info("store data size: %d", required_size);
    err = nvs_set_blob(my_handle, key, dataStore, required_size);

    if (err != ESP_OK)
    {
        log_err("nvs_set_blob error %d", err);
        return false;
    }

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        log_err("nvs_commit error");
        return false;
    }
    // Close
    nvs_close(my_handle);
    log_info("Data is saved\n");
    return true;
}

bool FlashHandler_getDeviceInfoInStore()
{
    DataIdInfo_t deviceInfo;
    if(FlashHandler_getData(NAMESPACE_GENARAL,KEY_DEVICE_INFO_ID_PWS,&deviceInfo))
    {
        log_info("got data in store id:%s, pw: %s",deviceInfo.productId,deviceInfo.password);
        strcpy(g_password,deviceInfo.password);
        strcpy(g_product_Id,deviceInfo.productId);
        return true;
    }
    else
    {
        return false;
    }
    
}
bool FlashHandler_saveDeviceInfoInStore()
{
    DataIdInfo_t deviceInfo;
    strcpy(deviceInfo.productId,g_product_Id);
    strcpy(deviceInfo.password,g_password);
    if(FlashHandler_setData(NAMESPACE_GENARAL,KEY_DEVICE_INFO_ID_PWS,&deviceInfo,sizeof(deviceInfo)))
    {
        log_info("save data in store done id:%s, pw: %s",deviceInfo.productId,deviceInfo.password);
        return true;
    }
    else
    {
        log_err("save device info false");
        return false;
    }
}

bool FlashHandler_getComMode()
{
    ComMode_t comMode;
    if(FlashHandler_getData(NAMESPACE_GENARAL,KEY_COM_MODE,&comMode))
    {
        log_info("got com mode in store: %d",comMode);
        g_comMode = comMode;
        return true;
    }
    else
    {
        log_info("commode have not set yet");
        return false;
    }
    
}
bool FlashHandler_saveComMode()
{
    ComMode_t comMode = g_comMode;
    if(FlashHandler_setData(NAMESPACE_GENARAL,KEY_COM_MODE,&g_comMode,sizeof(g_comMode)))
    {
        log_info("save com mode [%d] in store done",comMode);
        return true;
    }
    else
    {
        log_err("save com mode false");
        return false;
    }
    
}
extern uint8_t g_registerStatus;
bool FlashHandler_getRegisterStt()
{
    uint8_t regStt = NOT_REGISTER;
    if(FlashHandler_getData(NAMESPACE_GENARAL,KEY_REGISTER_STATUS,&regStt))
    {
        log_info("got register status in store: %d",regStt);
        g_registerStatus = regStt;
        return true;
    }
    else
    {
        log_info("register status have not set yet");
        return false;
    }
    
}
bool FlashHandler_saveRegisterStt()
{
    uint8_t regStt = g_registerStatus;
    if(FlashHandler_setData(NAMESPACE_GENARAL,KEY_REGISTER_STATUS,&regStt,sizeof(regStt)))
    {
        log_info("save register status [%d] in store done",regStt);
        return true;
    }
    else
    {
        log_err("save register status false");
        return false;
    }  
}


extern uint8_t g_newFirmMcuOta;
bool FlashHandler_getMcuOtaStt()
{
    uint8_t mcuFirmStt = NO_HAD_FIRM_MCU;
    if(FlashHandler_getData(NAMESPACE_GENARAL,KEY_FIRM_MCU_STATUS,&mcuFirmStt))
    {
        log_info("got mcu firm ota status in store: %d",mcuFirmStt);
        g_newFirmMcuOta = mcuFirmStt;
        return true;
    }
    else
    {
        log_info("mcu firm ota status have not set yet");
        return false;
    }
    
}
bool FlashHandler_saveMcuOtaStt()
{
    uint8_t mcuFirmStt = g_newFirmMcuOta;
    if(FlashHandler_setData(NAMESPACE_GENARAL,KEY_FIRM_MCU_STATUS,&mcuFirmStt,sizeof(mcuFirmStt)))
    {
        log_info("save mcu firm ota status [%d] in store done",mcuFirmStt);
        return true;
    }
    else
    {
        log_err("save mcu firm ota status false");
        return false;
    }  
}


extern uint8_t mcuPassWord[32];
bool FlashHandler_getMcuPassword()
{
    uint8_t mcuPw[32];
    if(FlashHandler_getData(NAMESPACE_GENARAL,KEY_PW_MCU,(void*)mcuPw))
    {
        log_info("got mcu password in store ok");
        memcpy(mcuPassWord, mcuPw, 32);
        return true;
    }
    else
    {
        log_info("mcu password have not set yet");
        return false;
    }
    
}
bool FlashHandler_saveMcuPassword()
{
    if(FlashHandler_setData(NAMESPACE_GENARAL,KEY_PW_MCU,mcuPassWord,sizeof(mcuPassWord)))
    {
        log_info("save mcu password status in store done");
        return true;
    }
    else
    {
        log_err("save mcu password status false");
        return false;
    }  
}




void FlashHandle_initInfoPwd()
{
    if (!FlashHandler_getDeviceInfoInStore())
    {
        // int seed = 0;       
        // seed = *((int*)g_macDevice); //int 4 byte thap
        // Utility_generateRamdomStr(g_password, PASSWORD_LEN - 1,seed);
        FlashHandler_saveDeviceInfoInStore();      //luu pwd random, id default la MAC nen phai goi sau ham get MAC, khi scan ma vach thi change id
    }
    if(!FlashHandler_getRegisterStt())
    {
        FlashHandler_saveRegisterStt();  //defaule NOT_REGISTER
    }
    if(!FlashHandler_getMcuOtaStt())
    {
        FlashHandler_saveMcuOtaStt();  //defaule NOT_REGISTER
    }
}



extern mqtt_certKey_t certKeyMqtt;
bool FlashHandler_getCertKeyMqttInStore()
{
    if(FlashHandler_getData(NAMESPACE_GENARAL,KEY_CERTKEY_MQTT,&certKeyMqtt))
    {
        // printf("got cert :%s, ",(char*)(certKeyMqtt.cert));
        // printf("got key :%s, ",(char*)(certKeyMqtt.key));
        return true;
    }
    else
    {
        printf("get cert key fail\n");
        return false;
    }
    
}
bool FlashHandler_saveCertKeyMqttInStore()
{
    if(FlashHandler_setData(NAMESPACE_GENARAL,KEY_CERTKEY_MQTT,&certKeyMqtt,sizeof(certKeyMqtt)))
    {
        log_info("save cert key mqtt ok");
        return true;
    }
    else
    {
        log_err("save cert key false");
        return false;
    }
    
}

extern mqtt_environment_t envir;
bool FlashHandler_getEnvironmentMqttInStore()
{
    if(FlashHandler_getData(NAMESPACE_GENARAL,KEY_ENVIR_MQTT,&envir))
    {
        printf("got environment :%s, ",(char*)(envir.environment));
        return true;
    }
    else
    {
        printf("get environment fail\n");
        return false;
    }
    
}
bool FlashHandler_saveEnvironmentMqttInStore()
{
    if(FlashHandler_setData(NAMESPACE_GENARAL,KEY_ENVIR_MQTT,&envir,sizeof(mqtt_environment_t)))
    {
        log_info("save environment mqtt ok");
        return true;
    }
    else
    {
        log_err("save environment false");
        return false;
    }
    
}

/***********************************************/
