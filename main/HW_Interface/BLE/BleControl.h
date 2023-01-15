/**
 ******************************************************************************
 * @file    Smarthome_HT_Gateway_W\main\template\header_file.h
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Sep 25, 2017
 * @brief   
 * @history
 ******************************************************************************/


#ifndef __BLE_CONTROL_H
#define __BLE_CONTROL_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void BLEC_processMess(char * mes);
void BLEC_sendPropertyToMobile(char *localId, char *value_iro);
void BLEC_processGetwater(char *mes);

#endif /* __BLE_CONTROL_H */
