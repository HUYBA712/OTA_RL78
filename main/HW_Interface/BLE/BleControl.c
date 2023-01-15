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
#include "BleControl.h"
#include "BLE_handler.h"
#include "cJSON.h"
#include "UART_Handler.h"
#include "MqttHandler.h"
#include "MKP_W_Protocol.h"
#include "HTG_Utility.h"
#include "IroControl.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#ifndef DISABLE_LOG_I
#define BLE_CONTROL_LOG_INFO_ON
#endif

#ifdef BLE_CONTROL_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI("BLE_CONTROL", format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#endif

#define log_error(format, ...) ESP_LOGE("BLE_CONTROL", format, ##__VA_ARGS__)

#define EVENT_TYPE_NEED_PROPERTIES "NP"
#define EVENT_TYPE_CONTROL_PROPERTY "CP"

#define MAX_LEN_MSG 150

#define GET_WATER_CMD_START "POUR:START:"
#define GET_WATER_CMD_GET "POUR:GET:"
#define GET_WATER_CMD_PAUSE "POUR:Pause"
#define GET_WATER_CMD_CONTINUE "POUR:Continue"
#define GET_WATER_CMD_CANCEL "POUR:Cancel"
#define GET_WATER_CMD_STATE "POUR:State"
#define GET_WATER_CMD_ERROR "POUR:ERROR"

/*******************************************************************************
 * Variables
 ******************************************************************************/
extern char topic_filter[4][30];   //

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
void BLEC_sendPropertyToMobile(char *localId, char *value_iro)
{
	char pData[MAX_LEN_MSG] = "";
	sprintf(pData, "{\"d\":%s,\"lid\":\"%s\",\"pc\":\"%s\"}", value_iro, localId, topic_filter[PROPERTY_CODE]);
	BLE_setPropertyData(topic_filter[PROPERTY_CODE],pData);
}

void BLEC_processMess(char *mes)
{
	cJSON *msgObject = cJSON_Parse(mes);
	if (msgObject == NULL)
	{
		log_info("CJSON_Parse unsuccess!");
		return;
	}
	if (cJSON_HasObjectItem(msgObject, "et")) //kiem tra xem lenh gui xuong qua mqtt co ..
	{										  //.. chua thong tin gi khac khong: info,reason,mid
		cJSON *et = cJSON_GetObjectItem(msgObject, "et");
		if (strcmp(et->valuestring, EVENT_TYPE_NEED_PROPERTIES) == 0)
		{
			log_info("ET: NP");
			UART_needUpdateAllParam();
		}
		else if (strcmp(et->valuestring, EVENT_TYPE_CONTROL_PROPERTY) == 0)
		{
			log_info("ET: CP");
			if(cJSON_HasObjectItem(msgObject,"pc"))
			{
				PROTO_processUpdateProperty(msgObject,cJSON_GetObjectItem(msgObject,"pc")->valuestring);
			}
		}
	}

	cJSON_Delete(msgObject);
}

void BLEC_processGetwater(char *mes)
{
	char* mesParam[10];
	uint8_t num ;
	if (startsWith(mes,GET_WATER_CMD_STATE))
	{
		Iro_sendGetStatusCmd();
		// BLE_sendToGetWaterChar("POUR:State:Free");
	}
// POUR:GET:$TYPE:$VOLUME:$TEMP
//     TYPE = HOT, COOL, COLD, RO
	else if (startsWith(mes,GET_WATER_CMD_GET))
	{
		num = convStringToParam(mes,(char **)mesParam,":");
		if (num <5)
		{
			log_error("get water miss param");
			return;
		}
		
		if (startsWith(mesParam[2],"HOT"))
		{
			Iro_sendGetWaterCmd(WATER_TYPE_HOT,atoi(mesParam[3]),atoi(mesParam[4]));
		}
		else if (startsWith(mesParam[2],"COOL"))
		{
			Iro_sendGetWaterCmd(WATER_TYPE_COOL,atoi(mesParam[3]),atoi(mesParam[4]));
		}
		else if (startsWith(mesParam[2],"COLD"))
		{
			Iro_sendGetWaterCmd(WATER_TYPE_COLD,atoi(mesParam[3]),atoi(mesParam[4]));
		}		
		else if (startsWith(mesParam[2],"RO"))
		{
			Iro_sendGetWaterCmd(WATER_TYPE_RO,atoi(mesParam[3]),atoi(mesParam[4]));
		}		
	}
	else if (startsWith(mes,GET_WATER_CMD_CANCEL))
	{
		Iro_sendCancelCmd();
	}	
	// POUR:START:$TYPE
	else if (startsWith(mes,GET_WATER_CMD_START))
	{
				num = convStringToParam(mes,(char **)mesParam,":");
		if (num <3)
		{
			log_error("start water miss param");
			return;
		}

		// Iro_sendStartWaterCmd();
	}	
}

/***********************************************/
