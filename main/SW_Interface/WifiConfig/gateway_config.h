/**
 ******************************************************************************
 * @file    Smarthome_HT_Gateway_W\main\gateway_config.h
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Sep 25, 2017
 * @brief
 * @history
 ******************************************************************************/

#ifndef __GATEWAY_CONFIG_H
#define __GATEWAY_CONFIG_H

/* Includes ------------------------------------------------------------------*/
#include "Global.h"
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

void GatewayConfig_start();
extern bool isWifiCofg;
void haveUserInfo(const char *userID);
void GatewayConfig_init();
void GatewayConfig_autoCfg();

//
#endif /* __GATEWAY_CONFIG_H */
