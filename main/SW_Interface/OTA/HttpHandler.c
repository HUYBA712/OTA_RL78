#include "HttpHandler.h"
#include <cJSON.h>
#include "Wifi_handler.h"
#include "DateTime.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "MqttHandler.h"

#ifndef DISABLE_LOG_I
#define HTTP_LOG_INFO_ON
#endif

#ifdef HTTP_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI("HTTP_Handler", format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#endif

/**
 * BasicHTTPClient.ino
 *
 *  Created on: 24.05.2015
 *
 */

#define TIME_KEEP_TOCKEN 360000000 //MS - 100h
#define HTTP_HANDLER_TAG "HTTP_Handler"
#define MAX_LEN_TOCKEN 500
char accessToken[MAX_LEN_TOCKEN];
uint32_t timeGetTocken = 0;
extern char g_product_Id[PRODUCT_ID_LEN]; 
extern char g_password[PASSWORD_LEN];
extern uint16_t mcuFirmVer;
char *s_tokenDataStr;
uint16_t s_tokenDataOffset = 0;

char *s_scheduleDataStr;
uint16_t s_scheduleDataOffset = 0;


esp_err_t getTocken_callback(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            log_info( "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            log_info( "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            log_info( "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            // log_info( "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            log_info( "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // printf("%.*s", evt->data_len, (char*)evt->data);
                memcpy(s_tokenDataStr + s_tokenDataOffset,evt->data,evt->data_len);
                s_tokenDataOffset += evt->data_len;
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

bool Http_getDeviceTocken()
{
    bool result = false;
    if (((esp_log_timestamp() - timeGetTocken) < TIME_KEEP_TOCKEN) && (timeGetTocken != 0))
        return true;
    char *postData = (char *)calloc(200, 1);
    s_tokenDataStr = (char *)calloc(MAX_LEN_TOCKEN + 15, 1);
    s_tokenDataOffset = 0;
    esp_http_client_config_t config = {
        .url = URL_GET_TOCKEN,
        .event_handler = getTocken_callback,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client,"Content-Type","application/json");
    esp_http_client_set_header(client,"Accept","application/json");
    sprintf(postData, "{\"strategy\": \"local\",\"productId\": \"%s\",\"password\": \"%s\"}", g_product_Id, g_password);
    esp_http_client_set_post_field(client, postData, strlen(postData));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info( "getDeviceTocken Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201)
        {
                    cJSON *root = cJSON_Parse(s_tokenDataStr);
                    if(root != NULL)
                    {
                        if(cJSON_HasObjectItem(root, "accessToken"))
                        {
                            cJSON *accessTokenOb = cJSON_GetObjectItem(root, "accessToken");
                            strcpy(accessToken, accessTokenOb->valuestring);
                            log_info("accessToken:%s", accessToken);
                            timeGetTocken = esp_log_timestamp();
                            result = true;
                        }
                    }
                    cJSON_Delete(root);
        }
    } else {
        ESP_LOGE(HTTP_HANDLER_TAG, "getDeviceTocken request failed: %s", esp_err_to_name(err));
    }
    free(postData);
    free(s_tokenDataStr);
    s_tokenDataStr = NULL;
    esp_http_client_cleanup(client);
    return result;
}
bool Http_getTokenStr(char *tokenStr)
{
    if (!Http_getDeviceTocken())
    {
        ESP_LOGE(HTTP_HANDLER_TAG, "Can't get device tocken");
        return false;
    }
    strcpy(tokenStr,accessToken);
    return true;
}

static void addDeviceToUserTask(void *parms)
{
    char *userId = (char *)parms;
    log_info("addDeviceToUser begin");
    bool reqResult = false;
    for (uint8_t i = 0; i < 5; i++)
    {
        reqResult = Http_getDeviceTocken();
        if (reqResult)
            break;
    }
    if (!reqResult)
    {
        ESP_LOGE(HTTP_HANDLER_TAG, "Can't get device tocken");
        vTaskDelete(NULL);
    }

    int result = -1;
    char *postData = (char *)calloc(200, 1);
    esp_http_client_config_t config = {
        .url = URL_RELATIONS,
        .event_handler = NULL,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client,"Content-Type","application/json");
    esp_http_client_set_header(client,"Authorization",accessToken);
    sprintf(postData, "{\"parentId\":[\"%s\"],\"parentType\" : \"userId\"}", userId);
    esp_http_client_set_post_field(client, postData, strlen(postData));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info( "addDeviceToUser Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201)
        {
            result = 200;
        }
        else if (esp_http_client_get_status_code(client) == 409)
        {
            result = 409;
        }
    } else {
        ESP_LOGE(HTTP_HANDLER_TAG, "addDeviceToUser request failed: %s", esp_err_to_name(err));
    }
    free(postData);
    esp_http_client_cleanup(client);

    addDeviceToUserDone(result);

    vTaskDelete(NULL);
}
static TaskHandle_t xAddDeviceToUserTask = NULL;

void Http_addDeviceToUser(char *userId)
{
    xTaskCreate(addDeviceToUserTask, "addDeviceToUserTask", 10 * 1024, userId, 15, &xAddDeviceToUserTask);
}


esp_err_t getSchedule_callback(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            log_info( "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            log_info( "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            log_info( "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            // log_info( "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            log_info( "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
                // log_info("check schedule response:%.*s", evt->data_len, (char*)evt->data);
                memcpy(s_scheduleDataStr + s_scheduleDataOffset,evt->data,evt->data_len);
                s_scheduleDataOffset += evt->data_len;
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
bool Http_getScheduleList()
{
    bool result = false;
    if (!Http_getDeviceTocken())
    {
        log_info("Can't get device tocken---------------------------------");
        return false;
    }
    s_scheduleDataStr = (char *)calloc(500, 1);
    s_scheduleDataOffset = 0;
    esp_http_client_config_t config = {
        .url = URL_SCHEDULE,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = getSchedule_callback,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client,"Content-Type","application/json");
    esp_http_client_set_header(client,"Authorization",accessToken);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info( "Http_getScheduleList Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201)
        {
            result = true;
        }
    } else {
        log_info( "Http_getScheduleList request failed: %s", esp_err_to_name(err));
    }
    free(s_scheduleDataStr);
    s_scheduleDataStr = NULL;
    esp_http_client_cleanup(client);
    return result;
}


bool Http_sendFirmWareVer()
{
    static bool is_sentFirmVer = false;
    // if(is_sentFirmVer) return true;  // send firmware version only 1 time

    log_info("Send Firmware Version");
    bool result = false;
    if (!Http_getDeviceTocken())
    {
        log_info("Can't get device tocken---------------------------------");
        return false;
    }
    char *postData = (char *)calloc(200, 1);
    esp_http_client_config_t config = {
        .url = URL_DEVICE_INFO,
        .event_handler = NULL,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    esp_http_client_set_header(client,"Content-Type","application/json");
    esp_http_client_set_header(client,"Authorization",accessToken);

    if(mcuFirmVer != 0)
        sprintf(postData, "{\"firmwareVersion\":%d,\"hardwareVerOfFirm\":%d, \"mcuFirmVer\":\"%d\"}", FIRM_VER,HW_VER_NUMBER, mcuFirmVer);
    else sprintf(postData, "{\"firmwareVersion\":%d,\"hardwareVerOfFirm\":%d}", FIRM_VER,HW_VER_NUMBER);

    printf("%s\n", postData);
    esp_http_client_set_post_field(client, postData, strlen(postData));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info( "Http_sendFirmWareVer Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201)
        {
            result = true;
            is_sentFirmVer = true;
        }
    } else {
        ESP_LOGE(HTTP_HANDLER_TAG, "Http_sendFirmWareVer request failed: %s", esp_err_to_name(err));
    }
    free(postData);
    esp_http_client_cleanup(client);
    return result;
}

bool Http_sendDeviceLocation(char* lat, char* lon)
{
    log_info("Send Device loaction");
    bool result = false;
    if (!Http_getDeviceTocken())
    {
        log_info("Can't get device tocken---------------------------------");
        return false;
    }
    char *postData = (char *)calloc(200, 1);
    esp_http_client_config_t config = {
        .url = URL_DEVICE_INFO,
        .event_handler = NULL,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    esp_http_client_set_header(client,"Content-Type","application/json");
    esp_http_client_set_header(client,"Authorization",accessToken);
    sprintf(postData, "{\"location\":{\"coordinates\":[%s,%s]}}", lat,lon);
    esp_http_client_set_post_field(client, postData, strlen(postData));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info( "sendDeviceLocation Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201)
        {
            result = true;
        }
    } else {
        ESP_LOGE(HTTP_HANDLER_TAG, "sendDeviceLocation request failed: %s", esp_err_to_name(err));
    }
    free(postData);
    esp_http_client_cleanup(client);

    return result;
}


extern mqtt_certKey_t *pMqttCertKey;
extern uint16_t lenCert;
extern uint16_t lenKey;
esp_err_t downloadCerificate_callback(esp_http_client_event_t *evt)
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
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
                memcpy(pMqttCertKey->cert + lenCert,evt->data,evt->data_len);
                lenCert += evt->data_len;
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

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");
bool Http_downloadFileCertificate(char *linkFile)
{
    // EWD_enableHardTimer();
    bool result = false;
    esp_http_client_config_t config = {
        .url = linkFile,//"https://tecomen-iotp-dev-s3-certificate.s3-ap-southeast-1.amazonaws.com/5f0dd1e5b3-certificate.pem.crt",
        .event_handler = downloadCerificate_callback,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        // .cert_pem = howsmyssl_com_root_cert_pem_start,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info( "download ceritficate file Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201)
        {
            result = true;
            pMqttCertKey->cert[lenCert] = '\0';
            log_info( "download ceritficate file OK");
        }
    } else {
        log_info( "download ceritficate file failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    // EWD_disableHardTimer();
    return result;
}

esp_err_t downloadCKey_callback(esp_http_client_event_t *evt)
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
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
                memcpy(pMqttCertKey->key + lenKey,evt->data,evt->data_len);
                lenKey += evt->data_len;
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

bool Http_downloadFileKeyMqtt(char *linkFile)
{
    // EWD_enableHardTimer();
    bool result = false;
    esp_http_client_config_t config = {
        .url = linkFile,//"https://tecomen-iotp-dev-s3-certificate.s3-ap-southeast-1.amazonaws.com/5f0dd1e5b3-private.pem.key",
        .event_handler = downloadCKey_callback,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        // .cert_pem = howsmyssl_com_root_cert_pem_start,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        log_info( "download key file Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200 || esp_http_client_get_status_code(client) == 201)
        {
            result = true;
            pMqttCertKey->key[lenKey] = '\0';
            log_info( "download key file OK");
        }
    } else {
        log_info( "download key file failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    // EWD_disableHardTimer();
    return result;
}

