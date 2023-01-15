#include "WatchDog.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "Wifi_handler.h"
#include "gateway_config.h"
#include "HTG_Utility.h"
#include "timeCheck.h"
#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL_SEC   (0.2)   // sample test interval for the second timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload
#define MAX_TIME_NOT_CONNECTED 60000 // tick

#define WATCHDOG_TIMER  TIMER_1
volatile bool DRAM_ATTR watchdogIoLevel = false;

bool s_needResart = false;
extern bool g_enTest;
void IRAM_ATTR timer_group0_isr(void *para)
{
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    int timer_idx = (int) para;

    if ((intr_status & BIT(timer_idx)) && timer_idx == WATCHDOG_TIMER) {
        // Clear the interrupt
        TIMERG0.int_clr_timers.t1 = 1;
        watchdogIoLevel = !watchdogIoLevel;
        // togle watchdog gpio
        // không thể dùng hàm gpio_set_level vì nó sẽ làm trương chình crash khi chạy các funtion flash.
        // funtion flash được dùng khi chạy OTA
        if(watchdogIoLevel)
        {
            GPIO.out_w1ts = (1 << WATCHDOG_GPIO);
        }else
        {
            GPIO.out_w1tc = (1 << WATCHDOG_GPIO);
        }
                
    }
    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG0.hw_timer[1].config.alarm_en = TIMER_ALARM_EN;
}

static void EWD_timer_init(int timer_idx, bool auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = auto_reload;
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr, 
        (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

}

void EWD_hardRestart()
{ 
    printf("Hard Reset\n");
    s_needResart = true;
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    if(g_enTest) return;
    if (!isWifiCofg)    esp_restart();
}
static void external_watchdog_task(void *pvParameter)
{
    while (1)
    {
        if((!s_needResart)  || (isWifiCofg && !g_enTest))
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            gpio_set_level(WATCHDOG_GPIO, 1);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            gpio_set_level(WATCHDOG_GPIO, 0);
        }
    }
}

bool wifiConnectedFirstTimes = false;
static void EWD_watchWifiTask(void *pram)
{
    uint32_t timeWifiDisconected = 0;
    while(1)
    {
        vTaskDelay(2000/portTICK_PERIOD_MS);
        // don't check wifi in ble mode. 
        if (g_comMode == COM_MODE_BLE)
        {
            timeWifiDisconected = 0;
            continue;
        }
         
        if (getWifiState() != Wifi_State_Got_IP)
        {
            if(timeWifiDisconected == 0)
            {
                timeWifiDisconected = xTaskGetTickCount();
            }
            else if (wifiConnectedFirstTimes && (!isWifiCofg) && (elapsedTime(xTaskGetTickCount(), timeWifiDisconected) > MAX_TIME_NOT_CONNECTED) )
            {
                printf("time out wifi disconnect after connected\n");
                EWD_hardRestart();
            }
            else if(!wifiConnectedFirstTimes && (!isWifiCofg) && (elapsedTime(xTaskGetTickCount(), timeWifiDisconected) > (60*MAX_TIME_NOT_CONNECTED)) )
            {
                printf("time out wifi disconnect after reset\n");
                EWD_hardRestart();
            }
        }else{
            timeWifiDisconected = 0;
        }
        
    }
}
void EWD_init(void)
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;

    uint64_t outputPinSel = ((uint64_t)1 << WATCHDOG_GPIO);

    io_conf.pin_bit_mask = outputPinSel;
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    xTaskCreate(&external_watchdog_task, "external_watchdog_task", 1024, NULL,6, NULL);
    EWD_timer_init(WATCHDOG_TIMER, TEST_WITH_RELOAD,    TIMER_INTERVAL_SEC);

    // xTaskCreate(&EWD_watchWifiTask, "EWD_watchWifiTask", 1024, NULL,1, NULL);
}

void EWD_enableHardTimer()
{
    timer_start(TIMER_GROUP_0, WATCHDOG_TIMER);
}

void EWD_disableHardTimer()
{
    timer_pause(TIMER_GROUP_0, WATCHDOG_TIMER);
}
