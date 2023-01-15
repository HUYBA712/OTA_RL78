#include "DateTime.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "lwip/err.h"
#include "lwip/apps/sntp.h"
#include "Wifi_handler.h"
#include "MqttHandler.h"
#include "HttpHandler.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "MKP_W_Protocol.h"
#include "cJSON.h"
//////
#ifndef DISABLE_LOG_ALL
#define DATE_TIME_LOG_INFO_ON
#endif

#ifdef DATE_TIME_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI("DateTime", format, ##__VA_ARGS__)
#define log_err(format, ...) ESP_LOGE("DateTime", format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#define log_err(format, ...)
#endif

static const char *TAG = "DateTime";
#define SCHEDULE_FILE "/spiflash/schedule.txt"
#define MAX_SCHEDULE_STR_LEN 14048 //BYTE

bool needSendFirmVer = false;
bool needSendFirmVerMqtt = false;
static void initialize_sntp(void)
{
    log_info("Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(0, "time.google.com");
    sntp_init();
}

static void obtain_time(void)
{
    initialize_sntp();
    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
    {
        log_info("Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

void setTimeZone()
{
    time_t now;
    struct tm timeinfo;
    time(&now);

    char strftime_buf[64];
    // Set timezone e
    setenv("TZ", "UTC-07:00", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    log_info("The current date/time in VN is: %s", strftime_buf);
}
extern EventGroupHandle_t wifi_event_group;
time_t timeStartSystem;
void getTimeStartStr(char *startTimeStr, uint8_t length)
{
    struct tm timeinfo;
    localtime_r(&timeStartSystem, &timeinfo);
    strftime(startTimeStr, length, "%c", &timeinfo);
    log_info("Started time is: %s", startTimeStr);
}
void getTimeNowStr(char *nowStr, uint8_t length)
{
    struct tm timeinfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(nowStr, length, "%c", &timeinfo);
    log_info("Now time is: %s", nowStr);
}
static void dateTimeTask(void *arg)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    // Set timezone e
    setenv("TZ", "UTC-07:00", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    log_info("The current date/time in device is: %s", strftime_buf);
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    log_info("Time is not set yet. Connecting to WiFi and getting time over NTP.");
    obtain_time();
    // update 'now' variable with current time
    setTimeZone();
    time(&now);
    localtime_r(&now, &timeinfo);
    timeStartSystem = now;
    while (1)
    {

        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        // Is time set? If not, tm_year will be (1970 - 1900).
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year < (2016 - 1900))
        {
            log_info("Time is not set yet. ----------------------------------------------------");
            sntp_stop();
            obtain_time();
            // update 'now' variable with current time
            setTimeZone();
            time(&now);
            localtime_r(&now, &timeinfo);
            
            vTaskDelay(500 / portTICK_RATE_MS);
        }
        else
        {
            vTaskDelay(4000 / portTICK_RATE_MS);
        }
    }
}

void initDateTime()
{
    xTaskCreate(dateTimeTask, "dateTimeTask", 10 * 1024, NULL, 5, NULL);
}


const char *base_path = "/spiflash";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

//--------------------
void mount_fs()
{
    log_info("Mounting FAT filesystem");
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formated before
    const esp_vfs_fat_mount_config_t mount_config = {
        format_if_mount_failed : true,
        max_files : 4,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "=====================");
        ESP_LOGE(TAG, "Failed to mount FATFS (0x%x)", err);
        ESP_LOGE(TAG, "=====================\r\n");
        return;
    }
    log_info("FATFS mounted.");
}