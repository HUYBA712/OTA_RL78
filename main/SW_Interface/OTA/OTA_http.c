#include "OTA_http.h"
#include <cJSON.h>
#include "Wifi_handler.h"
#include "esp_ota_ops.h"
#include "nvs.h"
#include "HttpHandler.h"
#include "WatchDog.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "BLD_mcu.h"
#include "UART_Handler.h"
#include "FlashHandle.h"
#include "BLD_mcuVoice.h"
#include "MqttHandler.h"
#include "BLD_mcu_rx.h"
#ifndef DISABLE_LOG_I
#define HTTP_LOG_INFO_ON
#endif

#ifdef HTTP_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI("OTA_Http", format, ##__VA_ARGS__)
#define log_warning(format, ...) ESP_LOGW("OTA_Http", format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#define log_warning(format, ...)

#endif


#define TIME_DELAY_START_CHECK_UPDATE 36000 //10h after connected wifi
extern uint8_t g_newFirmMcuOta;
uint32_t cntDelay_s = 0;
uint8_t otaPhase = PHASE_OTA_NONE;
esp_ota_handle_t update_handle = 0;
const esp_partition_t *update_partition = NULL;
bool hasOtaTask = false;
bool hasOtaTaskV2 = false;
extern bool isWifiCofg;
extern uint16_t mcuFirmVer;
extern uint16_t mcuHardVer;
extern uint16_t ra_mcu_firm;
extern bool needSendFirmVer; 
extern bool needSendFirmVerMqtt; 
extern bool g_isMqttConnected;
char s_linkDownLoad[210];
char s_linkDownLoadMcu[210];
uint64_t s_lastTimeUpdateFirm = 1601611951592;
bool s_needUpdateFw = false;
bool s_needUpdateFwMcu = false;
uint32_t s_otaMcuRx130Offset = 0; //dung cho ca RA6M1
static void otaTask_v2(void *arg);
char *s_checkUpdateDataStr = NULL;
uint16_t s_checkUpdateDataOffset = 0;
extern bool needSendFirmVer;

bool isCheckVoiceUpdate = false;
bool downloadUpdateFile(char *linkFile, uint64_t firmwaveTime);
void initOTA();
void endOTA();

uint64_t getLastTimeUpdate()
{
    nvs_handle my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        log_info("Error (%d) opening NVS handle!\n", err);
    }
    else
    {
        log_info("open storage Done\n");
        // Read
        log_info("Reading restart counter from NVS ... ");
        uint64_t lastTimeUp = 0; // value will default to 0, if not set yet in NVS
        err = nvs_get_u64(my_handle, "lastTimeUp", &lastTimeUp);
        switch (err)
        {
        case ESP_OK:
            log_info("Done\n");
            log_info("last update is %llu\n", lastTimeUp);
            nvs_close(my_handle);
            return lastTimeUp;
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            log_info("The value is not initialized yet!\n");
            nvs_close(my_handle);
            return 0;
            break;
        default:
            log_info("Error (%d) reading!\n", err);
        }
        // Close
        nvs_close(my_handle);
    }
    return 0;
}
bool setLastTimeUpdate(uint64_t firmwaveTime)
{
    nvs_handle my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        log_info("Error (%d) opening NVS handle!\n", err);
        return false;
    }
    else
    {
        log_info("open storage Done\n");
        err = nvs_set_u64(my_handle, "lastTimeUp", firmwaveTime);
        printf("Committing updates: %llu in NVS ... \n",firmwaveTime);
        err = nvs_commit(my_handle);
        if (err != ESP_OK)
        {
            printf("Failed!\n");
            nvs_close(my_handle);
            return false;
        }
        else 
        {
            printf("Done\n");
            nvs_close(my_handle);
        }
    }
    return true;
}

esp_err_t OTA_checkUpdate_cb(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            log_info( "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            log_info( "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            // log_info( "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            // log_info( "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            log_info( "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
                log_info("check update response:%.*s", evt->data_len, (char*)evt->data);
                memcpy(s_checkUpdateDataStr + s_checkUpdateDataOffset,evt->data,evt->data_len);
                s_checkUpdateDataOffset += evt->data_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            log_info( "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            log_info( "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                log_info( "Last esp error code: 0x%x", err);
                log_info( "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
    }
    return ESP_OK;
}

bool Ota_http_checkUpdate()
{
    // get token
    char *tokenStr = (char *)calloc(MAX_LEN_TOCKEN, 1);
    if(!Http_getTokenStr(tokenStr))
    {
        free(tokenStr);
        return false;
    }
    bool result = false;
    s_checkUpdateDataStr = (char *)calloc(500, 1);
    s_checkUpdateDataOffset = 0;
    char *linkCheck = (char *)calloc(200, 1);
    // sprintf(linkCheck, "%s?deviceType=%s&hardwareVersion=%d&time=%llu", URL_CHECK_UPDATE,DEVICE_TYPE_STR, HW_VER_NUMBER,getLastTimeUpdate());
    sprintf(linkCheck, "%s?deviceType=%s&hardwareVersion=%d&firmwareVersion=%d", URL_CHECK_UPDATE,DEVICE_TYPE_STR, HW_VER_NUMBER,FIRM_VER);
    esp_http_client_config_t config = {
        .url = linkCheck,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = OTA_checkUpdate_cb,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client,"Content-Type","application/x-www-form-urlencoded");
    esp_http_client_set_header(client,"Accept","application/json");
    esp_http_client_set_header(client,"Authorization",tokenStr);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info( "Ota_http_checkUpdate Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201)
        {
                    cJSON *root = cJSON_Parse(s_checkUpdateDataStr);
                    if(root != NULL)
                    {
                        if(cJSON_HasObjectItem(root, "total"))
                        {
                            int total = cJSON_GetObjectItem(root, "total")->valueint;
                            if (total)
                            {
                                cJSON *dataObject = cJSON_GetArrayItem(cJSON_GetObjectItem(root, "data"), 0);
                                s_linkDownLoad[0] = '\0';
                                strcat(s_linkDownLoad,URL_PRE_OTA_DOWNLOAD_LINK);
                                strcat(s_linkDownLoad, cJSON_GetObjectItem(dataObject, "file")->valuestring);
                                s_needUpdateFw = true;
                                // double timeFirmware = cJSON_GetObjectItem(dataObject, "createdAt")->valuedouble;
                                // log_info("time:----------------------------------------%lf", timeFirmware);
                                // downloadUpdateFile(link, (uint64_t)timeFirmware);
                            }
                        }
                        result = true;
                    }
                    cJSON_Delete(root);
        }
    } else {
        log_warning( "Ota_http_checkUpdate request failed: %s", esp_err_to_name(err));
    }
    free(tokenStr);
    free(linkCheck);
    free(s_checkUpdateDataStr);
    s_checkUpdateDataStr = NULL;
    esp_http_client_cleanup(client);
    return result;
}


bool Ota_http_checkUpdateMcu()
{
    // get token
    char *tokenStr = (char *)calloc(MAX_LEN_TOCKEN, 1);
    if(!Http_getTokenStr(tokenStr))
    {
        free(tokenStr);
        return false;
    }
    bool result = false;
    s_checkUpdateDataStr = (char *)calloc(500, 1);
    s_checkUpdateDataOffset = 0;
    char *linkCheck = (char *)calloc(200, 1);
    // sprintf(linkCheck, "%s?deviceType=%s&hardwareVersion=%d&time=%llu", URL_CHECK_UPDATE,DEVICE_TYPE_STR, HW_VER_NUMBER,getLastTimeUpdate());
    if(isCheckVoiceUpdate)
    {
        sprintf(linkCheck, "%s?deviceType=%s&hardwareVersion=%d&firmwareVersion=%d", URL_CHECK_UPDATE,DEVICE_TYPE_MCU_STR_VOICE, HW_VER_NUMBER,ra_mcu_firm);
    }
    else 
    {
        sprintf(linkCheck, "%s?deviceType=%s&hardwareVersion=%d&firmwareVersion=%d", URL_CHECK_UPDATE,DEVICE_TYPE_MCU_STR, HW_VER_NUMBER,mcuFirmVer);
    }   
    esp_http_client_config_t config = {
        .url = linkCheck,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = OTA_checkUpdate_cb,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client,"Content-Type","application/x-www-form-urlencoded");
    esp_http_client_set_header(client,"Accept","application/json");
    esp_http_client_set_header(client,"Authorization",tokenStr);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info( "Ota_http_checkUpdate %s Status = %d, content_length = %d", isCheckVoiceUpdate ? "MCU-VOICE" : "MCU",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201)
        {
                    cJSON *root = cJSON_Parse(s_checkUpdateDataStr);
                    if(root != NULL)
                    {
                        if(cJSON_HasObjectItem(root, "total"))
                        {
                            int total = cJSON_GetObjectItem(root, "total")->valueint;
                            if (total)
                            {
                                cJSON *dataObject = cJSON_GetArrayItem(cJSON_GetObjectItem(root, "data"), 0);
                                s_linkDownLoadMcu[0] = '\0';
                                strcat(s_linkDownLoadMcu,URL_PRE_OTA_DOWNLOAD_LINK);
                                strcat(s_linkDownLoadMcu, cJSON_GetObjectItem(dataObject, "file")->valuestring);
                                s_needUpdateFwMcu = true;
                                // double timeFirmware = cJSON_GetObjectItem(dataObject, "createdAt")->valuedouble;
                                // log_info("time:----------------------------------------%lf", timeFirmware);
                                // downloadUpdateFile(link, (uint64_t)timeFirmware);
                            }
                        }
                        result = true;
                    }
                    cJSON_Delete(root);
        }
    } else {
        log_warning( "Ota_http_checkUpdate request failed: %s", esp_err_to_name(err));
    }
    free(tokenStr);
    free(linkCheck);
    free(s_checkUpdateDataStr);
    s_checkUpdateDataStr = NULL;
    esp_http_client_cleanup(client);
    return result;
}

esp_err_t downloadUpdateFile_callback(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            log_info( "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            log_info( "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            // log_info( "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            // log_info( "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            // log_info( "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // printf("0");
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // printf("1");
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
                if(otaPhase == PHASE_OTA_ESP)
                {
                    esp_ota_write(update_handle, (const void *)evt->data, (size_t)evt->data_len);
                    vTaskDelay(2/portTICK_PERIOD_MS);
                }
                else if(otaPhase == PHASE_OTA_MCU)
                {
                    bld_binDataWriteToPartition(s_otaMcuRx130Offset, (void*)evt->data, (size_t)evt->data_len);
                    s_otaMcuRx130Offset += evt->data_len;
                    printf(" %d\n",s_otaMcuRx130Offset);
                }  
                else if(otaPhase == PHASE_OTA_MCU_VOICE)
                {
                    bldVoice_binDataWriteToPartition(s_otaMcuRx130Offset, (void*)evt->data, (size_t)evt->data_len);
                    s_otaMcuRx130Offset += evt->data_len;
                    printf(" %d\n",s_otaMcuRx130Offset);
                }   
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            log_info( "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            log_info( "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                log_info( "Last esp error code: 0x%x", err);
                log_info( "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
    }
    return ESP_OK;
}
bool downloadUpdateFile(char *linkFile, uint64_t firmwaveTime)
{
    EWD_enableHardTimer();
    if(otaPhase == PHASE_OTA_ESP)
    {
        initOTA();    
    }
    else if(otaPhase == PHASE_OTA_MCU || otaPhase == PHASE_OTA_MCU_VOICE)
    {
        bld_createGetPartitionFileDownload(otaPhase);
        s_otaMcuRx130Offset = 0;
    }
    else return false;
    log_info("start download file:%s", linkFile);
    bool result = false;

    esp_http_client_config_t config = {
        .url = linkFile,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = downloadUpdateFile_callback,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info( "downloadUpdateFile Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201)
        {
            if(otaPhase == PHASE_OTA_ESP)  
            {
                setLastTimeUpdate(firmwaveTime);
                endOTA();
            } 
            result = true;
        }
    } else {
        log_warning( "downloadUpdateFile request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    EWD_disableHardTimer();
    return result;
}

void initOTA()
{
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */

    log_info("Starting OTA ...");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running)
    {
        log_info("Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        log_info("(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    log_info("Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    update_partition = esp_ota_get_next_update_partition(NULL);
    log_info("Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK)
    {
        log_info("esp_ota_begin failed, error=%d", err);
        esp_restart();
    }
    log_info("esp_ota_begin succeeded");
}

void endOTA()
{
    if (esp_ota_end(update_handle) != ESP_OK)
    {
        log_info("esp_ota_end failed!");
        esp_restart();
    }
    esp_err_t err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        log_info("esp_ota_set_boot_partition failed! err=0x%x", err);
        esp_restart();
    }
    log_info("Prepare to restart system!");
    esp_restart();
    return;
}
bool OtaHttp_createDoOtaTask()
{
    if(!hasOtaTaskV2)
    {
        // xTaskCreate(otaTask_v2, "otaTask_v2", 4*1024, NULL, 5, NULL);
        return true;
    }
    return false;
}
static void otaTask(void *arg)
{
    bool isReset = true;
    // uint64_t lastTimeFirm = 0;
    // lastTimeFirm = getLastTimeUpdate();
    // printf("get last time update FIRMWARE: %llu\n",lastTimeFirm);
    // if(lastTimeFirm < s_lastTimeUpdateFirm)
    // {
    //     setLastTimeUpdate(s_lastTimeUpdateFirm);
    // }
    // else{
    //     s_lastTimeUpdateFirm = lastTimeFirm;
    // }
    WIFI_HANDLER_WAIT_CONECTED_NOMAL_FOREVER;
    while (1)
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
        if (isWifiCofg)
        {
            vTaskDelay(1000/portTICK_PERIOD_MS);
            continue;
        }
        uint32_t timeOffsetCheck = 0;
        if(isReset)
        {
            timeOffsetCheck = 10;
            isReset = false;
        }
        else{
            if(cntDelay_s++ < (2*TIME_DELAY_START_CHECK_UPDATE))
            {
                vTaskDelay(1000/portTICK_PERIOD_MS);
                continue;
            }
            else{
                cntDelay_s = 2*TIME_DELAY_START_CHECK_UPDATE; //stop delay
            }
            vTaskDelay(1000/portTICK_PERIOD_MS);

            srand((unsigned int)esp_log_timestamp());
            timeOffsetCheck = 5 + rand()%TIME_DELAY_START_CHECK_UPDATE;        //random in 10h then
        }
        
        cntDelay_s = 0;
        uint8_t cnt_retry = 0;
        printf("will check update after %d second\n", timeOffsetCheck);
        while(1)
        {
            if (isWifiCofg)
            {
                vTaskDelay(1000/portTICK_PERIOD_MS);
                continue;
            }
            if(cntDelay_s++ < timeOffsetCheck)
            {
                vTaskDelay(1000/portTICK_PERIOD_MS);
                continue;
            }
            if (Ota_http_checkUpdate())
            {
                vTaskDelay(1000/portTICK_PERIOD_MS);
                if(s_needUpdateFw)
                {
                    if(hasOtaTaskV2) 
                    {
                        log_info("task update running");
                    }
                    else{
                        otaPhase = PHASE_OTA_ESP;
                        OtaHttp_createDoOtaTask();
                    }
                }
                else
                {
                    // if(mcuFirmVer != 0)
                    // {
                        isCheckVoiceUpdate = false;
                        if(Ota_http_checkUpdateMcu())
                        {
                            vTaskDelay(1000/portTICK_PERIOD_MS);
                            if(s_needUpdateFwMcu)
                            {
                                if(hasOtaTaskV2) 
                                {
                                    log_info("task update running");
                                }
                                else{
                                    otaPhase = PHASE_OTA_MCU;
                                    printf("need download firmware mcu\n");
                                    OtaHttp_createDoOtaTask();
                                }        
                            }
                            else
                            {
                                isCheckVoiceUpdate = true;
                                if(Ota_http_checkUpdateMcu())
                                {
                                    vTaskDelay(1000/portTICK_PERIOD_MS);
                                    if(s_needUpdateFwMcu)
                                    {
                                        if(hasOtaTaskV2) 
                                        {
                                            log_info("task update running");
                                        }
                                        else{
                                            otaPhase = PHASE_OTA_MCU_VOICE;
                                            printf("need download firmware mcu voice\n");
                                            OtaHttp_createDoOtaTask();
                                        }
                                    }
                                }
                            }
                            s_needUpdateFwMcu = false;
                        }
                    // }

                }
                s_needUpdateFw = false;
                break;
            }
            else{
                printf("retry check update over http\n");
                cnt_retry++;
            }
            if(cnt_retry >= 3) break;
            vTaskDelay(20000 / portTICK_RATE_MS);
        }
        printf("continue check daily\n");
        cntDelay_s = 0;
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
    // hasOtaTask = false;
    vTaskDelete(NULL);
}
//-------------------
void OTA_http_checkAndDo()
{
    // if(!hasOtaTask){
    xTaskCreate(otaTask, "otaTask", 4*1024, NULL, 5, NULL);
    //     hasOtaTask = true;
    // }
}
extern bool g_mcuOtaRunning;
extern bool g_mcuVoiceOtaRunning;
uint16_t listMcuHardRx130[] = LIS_MCU_HARD_RX130;
uint16_t listMcuHardMsp[] = LIS_MCU_HARD_MSP;
static bool otaCheckHardMcu(bool *isMsp)
{
    printf("mcuHard: %d\n", mcuHardVer);
    for(int i = 0; i < sizeof(listMcuHardMsp)/sizeof(listMcuHardMsp[0]); i++)
    {
        if(mcuHardVer == listMcuHardMsp[i])
        {
            printf("match mcu MSP hard: %d\n",listMcuHardMsp[i]);
            *isMsp = true;
            return true;
        }
    }
    for(int i = 0; i < sizeof(listMcuHardRx130)/sizeof(listMcuHardRx130[0]); i++)
    {
        if(mcuHardVer == listMcuHardRx130[i])
        {
            printf("match mcu RX130 hard: %d\n",listMcuHardRx130[i]);
            *isMsp = false;
            return true;
        }
    }
    return false;
}
static void otaTask_v2(void *arg)
{
    hasOtaTaskV2 = true;
    //char * linkFile = (char *)arg;
    // char linkDownLoad[255] = {0};
    // strcpy(linkDownLoad,(char *)arg);
    // log_info("otaTask link: %s",linkDownLoad);
    WIFI_HANDLER_WAIT_CONECTED_NOMAL_FOREVER;
    uint8_t retryTime = 0;
    for (retryTime = 0; retryTime < 3; retryTime++)
    {
        if(otaPhase == PHASE_OTA_ESP)
        {
            if (downloadUpdateFile(s_linkDownLoad, s_lastTimeUpdateFirm))
                break;
        }
        else  if(otaPhase == PHASE_OTA_MCU || otaPhase == PHASE_OTA_MCU_VOICE)
        {
            if(g_mcuOtaRunning || g_mcuVoiceOtaRunning)
            {
                printf("ota busy....\n");
                break;
            }   
            if (downloadUpdateFile(s_linkDownLoadMcu, s_lastTimeUpdateFirm))
            {
                //set state HAVE_NEW_FIRM_MCU
                g_newFirmMcuOta = HAD_NEW_FIRM_MCU;
                if(FlashHandler_saveMcuOtaStt() == true)
                {
                    if(otaPhase == PHASE_OTA_MCU_VOICE)
                    {
                        bldMcuVoice_taskUpdate();
                    }
                    else  if(otaPhase == PHASE_OTA_MCU){
                        bool isMsp = false;
                        if(otaCheckHardMcu(&isMsp))
                        {
                            if(isMsp)
                            {
                                bldMcu_taskUpdate();
                            }
                            else
                            {
                                bldMcuRx_taskUpdate();
                            }
                        }
                        else
                        {
                            printf("no match mcu hard version\n");
                        }
                    }

                }
                //create task ota mcu
                break;
            }
                
        }
        
        vTaskDelay(10000 / portTICK_RATE_MS);
    }
    if(retryTime >= 3 && (otaPhase == PHASE_OTA_MCU || otaPhase == PHASE_OTA_MCU_VOICE))
    {
        if(otaPhase == PHASE_OTA_MCU_VOICE)
        {
            bldMcuVoice_updateStatusMqtt(BLD_VOICE_STT_DOWNLOAD_FAIL);
        }
        else  if(otaPhase == PHASE_OTA_MCU){
            bldMcu_updateStatusMqtt(BLD_STT_DOWNLOAD_FAIL);
        }
    }
    hasOtaTaskV2 = false;
    vTaskDelete(NULL);
}
void OTA_http_DoOTA(char *linkFile, uint8_t phase)
{
    if(hasOtaTaskV2) 
    {
        log_info("task update running");
        return;
    }
    log_info("DoOTA link: %s",linkFile);
    if(phase == PHASE_OTA_ESP)
    {
        s_linkDownLoad[0] = '\0';
	    strcpy(s_linkDownLoad,linkFile); 
    }
    else if(phase == PHASE_OTA_MCU)
    {
        s_linkDownLoadMcu[0] = '\0';
	    strcpy(s_linkDownLoadMcu,linkFile); 
    }
    else if(phase == PHASE_OTA_MCU_VOICE)
    {
        s_linkDownLoadMcu[0] = '\0';
	    strcpy(s_linkDownLoadMcu,linkFile); 
    }
    else 
       return;	
    otaPhase = phase;   
    OtaHttp_createDoOtaTask();
}