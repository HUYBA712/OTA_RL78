#include "OutputControl.h"
#include "MqttHandler.h"
#include "Gateway_config.h"
#include "UART_Handler.h"
#include <stdlib.h>


#ifndef DISABLE_LOG_ALL
#define OUTPUT_CONTROL_LOG_INFO_ON
#endif


#ifdef OUTPUT_CONTROL_LOG_INFO_ON
#define log_info(format, ... ) ESP_LOGI("Output_Control",  format , ##__VA_ARGS__)
#else
#define log_info(format, ... )
#endif

const gpio_num_t initOutputs[NUM_OF_OUTPUT] = {
	LED1,
};

const gpio_num_t ledTable[NUM_OF_LED] = {
	LED1,
};

void Out_initAllOutput()
{
	gpio_config_t io_conf;
	//disable interrupt
	io_conf.intr_type = GPIO_INTR_DISABLE;
	//set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	uint64_t outputPinSel = 0;
	for (int i = 0; i < NUM_OF_OUTPUT; ++i)
	{
		outputPinSel |= ((uint64_t)1 << initOutputs[i]);
	}
	io_conf.pin_bit_mask = outputPinSel;
	//disable pull-down mode
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	//disable pull-up mode
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	//configure GPIO with the given settings
	gpio_config(&io_conf);
	Out_setAllOutput(1);
}

void Out_setGpioState(gpio_num_t gpio_num, uint8_t state)
{
	gpio_set_level(gpio_num,state);
}


// For Led control
void Out_toggleAllLed()
{
	for (int i = 0; i < NUM_OF_LED; i++)
	{
		gpio_set_level(ledTable[i], !gpio_get_level(ledTable[i]));
	}
}
void Out_setAllLed(uint8_t state)
{
	for (int i = 0; i < NUM_OF_LED; i++)
	{
		gpio_set_level(ledTable[i], state);
	}
}
void Out_setAllOutput(uint8_t state)
{
	for (int i = 0; i < NUM_OF_OUTPUT; ++i)
	{
		gpio_set_level(initOutputs[i], state);
	}
}

extern bool OTA_run;
extern bool isWifiCofg;
extern bool g_enTest;
extern bool g_isMqttConnected;
static void led_task(void *pvParameter)
{
	//vTaskDelay(7000 / portTICK_RATE_MS);
	while(1)
	{
		if(isWifiCofg || g_enTest)
		{
			vTaskDelay(200 / portTICK_RATE_MS);
			continue;
		}

		if(g_isMqttConnected)
		{
			Out_setAllLed(1);
			vTaskDelay(200 / portTICK_RATE_MS);
			Out_setAllLed(0);
			vTaskDelay(1800 / portTICK_RATE_MS);
		}
		else
		{	
			Out_setAllLed(1);
			vTaskDelay(1000 / portTICK_RATE_MS);
		}
	}
	vTaskDelete(NULL);
}

void Out_LedIndicate()
{
	Out_initAllOutput();
	xTaskCreate(&led_task, "led_task", 2048, NULL, 5, NULL);
}

