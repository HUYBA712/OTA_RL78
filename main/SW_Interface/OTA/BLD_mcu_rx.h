#ifndef BLD_MCU_RX_H_
#define BLD_MCU_RX_H_
#include "BLD_mcu.h"
#define HAD_NEW_FIRM_MCU 1
#define NO_HAD_FIRM_MCU 0
/* **** Configuration of BootMode Entry **** */
// #define UB_PIN 17  /* UB# pin */
#define MD_PIN 32  /* MD pin */
#define RES_PIN_RX 33 /* RES# pin */


void bldMcuRx_pinInit(void);
void bldMcuRx_pinDeinit(void);
void bldMcuRx_bootModeEntry(void);
void bldMcuRx_bootModeRelease(void);
void bldMcuRx_updateStatusMqtt(bld_status_t stt);
void bldMcuRx_taskUpdate();
#endif