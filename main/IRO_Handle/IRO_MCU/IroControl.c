/**
 ******************************************************************************
 * @file    Smarthome_HT_Gateway_W\main\template\source_file.c
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Sep 25, 2017
 * @brief   
 * @history
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "IroControl.h"
#include "UART_Handler.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

 /*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
void Iro_SendOnlineCmd(char *data)
{
    char buf_data[60] = "";
    sprintf((char *)buf_data, "[ONLINE_CMD,%s]", data);
    printf(buf_data);
    printf("\n ----------------------\n");
    UART_sendToQueueToSend((char *)buf_data); //gui sang cho IRO
}


void Iro_sendGetWaterCmd(IroWaterType_t type, uint16_t volume, int8_t temp)
{
    char buf_data[60] = "";
    sprintf((char *)buf_data, "%d,%d,%d,%d",ONLINE_CMD_GET_WATER,type,volume,temp);
    Iro_SendOnlineCmd(buf_data);
}
// POUR:START:$TYPE
void Iro_sendStartWaterCmd(IroWaterType_t type)
{
    char buf_data[60] = "";
    sprintf((char *)buf_data, "%d,%d",ONLINE_CMD_START_WATER,type);
    Iro_SendOnlineCmd(buf_data);
}
void Iro_sendCancelCmd()
{
    char buf_data[60] = "";
    sprintf((char *)buf_data, "%d",ONLINE_CMD_CANCEL);
    Iro_SendOnlineCmd(buf_data);
}
void Iro_sendGetStatusCmd()
{
    char buf_data[60] = "";
    sprintf((char *)buf_data, "%d",ONLINE_CMD_GET_STATUS);
    Iro_SendOnlineCmd(buf_data);
}
/***********************************************
lấy nước:
[ONLINE_CMD,1,type,volume,temp]
type: 
typedef enum{
    WATER_TYPE_HOT = 0,
    WATER_TYPE_COOL = 1,
    WATER_TYPE_COLD = 2,
    WATER_TYPE_RO = 3,    
} IroWaterType_t;

vd: [ONLINE_CMD,1,0,200,89]
***********************************************/

/***********************************************
lấy nước không có thể tích:
[ONLINE_CMD,2,type]
type: 
typedef enum{
    WATER_TYPE_HOT = 0,
    WATER_TYPE_COOL = 1,
    WATER_TYPE_COLD = 2,
    WATER_TYPE_RO = 3,    
} IroWaterType_t;

vd: [ONLINE_CMD,1,0]
***********************************************/

/***********************************************
hủy lấy nước:
[ONLINE_CMD,3]
***********************************************/
/***********************************************
hỏi trạng thái:
[ONLINE_CMD,4]
***********************************************/
/***********************************************
hủy lấy nước:
[ONLINE_CMD,3]
***********************************************/