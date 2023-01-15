/**
 ******************************************************************************
 * @file    Smarthome_HT_Gateway_W\main\template\header_file.h
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Sep 25, 2017
 * @brief   
 * @history
 ******************************************************************************/


#ifndef __IRO_CONTROL_H
#define __IRO_CONTROL_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"
/* Exported types ------------------------------------------------------------*/
typedef enum{
    WATER_TYPE_HOT = 0,
    WATER_TYPE_COOL = 1,
    WATER_TYPE_COLD = 2,
    WATER_TYPE_RO = 3,    
} IroWaterType_t;

typedef enum
{
	WATER_STATE_FREE = 0,
	WATER_STATE_BUSY,
	WATER_STATE_DONE
} voidceCmdWaterState_t;
enum{
    ONLINE_CMD_GET_WATER = 1,
    ONLINE_CMD_START_WATER = 2,
    ONLINE_CMD_CANCEL = 3,
    ONLINE_CMD_GET_STATUS = 4,
};
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void Iro_sendGetStatusCmd();
void Iro_sendCancelCmd();
void Iro_sendStartWaterCmd(IroWaterType_t type);
void Iro_sendGetWaterCmd(IroWaterType_t type, uint16_t volume, int8_t temp);

#endif /* __IRO_CONTROL_H */
