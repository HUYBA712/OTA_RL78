#if !defined(WATCH_DOG_H)
#define WATCH_DOG_H

#include "Global.h"
#include "driver/gpio.h"

#if defined(HW_VER_MSP_VOICE) || defined(HW_VER_MSP430) || defined(HW_VER_3_0) || defined(HW_VER_3_1) || defined(HW_VER_2_1)
#define WATCHDOG_GPIO GPIO_NUM_14
#elif defined(HW_VER_2_0)
#define WATCHDOG_GPIO GPIO_NUM_12
#endif

void EWD_init(void);
void EWD_hardRestart();
void EWD_enableHardTimer();
void EWD_disableHardTimer();
#endif // WATCH_DOG_H
