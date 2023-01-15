#ifndef OUTPUT_CONTROL_H
#define OUTPUT_CONTROL_H
#include "Global.h"
#include "driver/gpio.h"

#if defined(HW_VER_MSP_VOICE) || defined(HW_VER_MSP430) || defined(HW_VER_3_0) || defined(HW_VER_3_1) || defined(HW_VER_2_1)
#define LED1 GPIO_NUM_12
#elif defined(HW_VER_2_0)
#define LED1 GPIO_NUM_14
#endif

// 18 19 21 v2
// 15 18 19 v3.1.2

#define NUM_OF_OUTPUT 1
#define NUM_OF_LED 1

void Out_initAllOutput();
void Out_setGpioState(gpio_num_t gpio_num, uint8_t state);
void Out_toggleAllLed();
void Out_setAllLed(uint8_t state);
void Out_setAllOutput(uint8_t state);
void Out_LedIndicate();

#endif