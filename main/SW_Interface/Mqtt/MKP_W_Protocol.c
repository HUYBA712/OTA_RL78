#include "MKP_W_Protocol.h"
#include "OutputControl.h"
#include "DateTime.h"
#include "MqttHandler.h"
#include "esp32/rom/rtc.h"
#include "OTA_http.h"
#include "UART_Handler.h"
#include "HTG_Utility.h"
#include "MqttHandler.h"
#include "HttpHandler.h"
#include "FlashHandle.h"
#include "BLE_handler.h"
#ifndef DISABLE_LOG_ALL
#define MKP_W_Protocol_LOG_INFO_ON
#endif

#ifdef MKP_W_Protocol_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI("MKP_W_Protocol", format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#endif

#define TAG "MKP_W_Protocol"

#define EVT_CONTROL_PROPERTY "CP"
#define EVT_SCHEDULE_CHANGE "SC"
#define EVT_UPDATE_FIRMWARE "UF"
extern char topic_filter[4][30];
extern uint16_t mcuHardVer;
// char linkDownLoad[255] = {0};
/*SmartBed*/
typedef uint8_t (*parse_msg_t)(void *);
typedef struct
{
	const char *cmdType;
	parse_msg_t msgHandle;
} mqtt_in_msg_t;
// message handler
uint8_t PROTO_CtrAtt_IroHsd(void *data);
uint8_t PROTO_CtrAtt_IroRtt(void *data);
uint8_t PROTO_CtrATT_IroRsft(void *data);
uint8_t PROTO_CtrATT_ctrlDisp(void *data);
uint8_t PROTO_CtrATT_ctrlHot(void *data);
uint8_t PROTO_CtrATT_ctrlCold(void *data);
//---------------------------------------Message handler

#define PROTO_IROHSD "iro_hsd" //
#define PROTO_IRORTT "iro_rtt"
#define PROTO_IRORSFT "iro_rsft"
#define PROTO_IROCTRLDISP "ctrl_disp"
#define PROTO_IROCTRLHOT "ctrl_hot"
#define PROTO_IROCTRLCOLD "ctrl_cold"
#define PROTO_NUM_OF_IN_MSG 6
const mqtt_in_msg_t mqttInMsgHandleTable[PROTO_NUM_OF_IN_MSG] = {
	{PROTO_IROHSD, PROTO_CtrAtt_IroHsd},
	{PROTO_IRORTT, PROTO_CtrAtt_IroRtt},
	{PROTO_IRORSFT, PROTO_CtrATT_IroRsft},
	{PROTO_IROCTRLDISP, PROTO_CtrATT_ctrlDisp},
	{PROTO_IROCTRLHOT, PROTO_CtrATT_ctrlHot},
	{PROTO_IROCTRLCOLD, PROTO_CtrATT_ctrlCold}
};

//--------------------------------------------------------------------------------------------- message handler

/*MKP IRO
IRO  chi su dung localId = 1, va localId nay co cac property code duoc define ben duoi
Example topic name:   d/b4e62dacc3d5/s/buivietanh96/CP/1/iro_vx
				      d/b4e62dacc3d5/s/buivietanh96/CP/1/iro_rtt
	
					  ...
----------------------d/WIFI MAC esp/s/--username--/EventType/LocalId/PropertyCode
Example data:
{
	{"d":"1"}
}

*/
/*function gui dữ liệu nhận từ mqtt server cho IRO tương ứng với cac lệnh R/W
	num_filters: số lõi lọc*/
uint8_t num_filters = 0;
uint8_t PROTO_CtrAtt_IroRtt(void *data)
{
	if (data != NULL)
	{
		char buf_data[100] = "";
		sprintf((char *)buf_data, "[UP,iro_rtt,%d,%s]", num_filters, (char *)data);
		UART_sendToQueueToSend((char *)buf_data); //gui sang cho IRO
	}
	return 0;
}

uint8_t PROTO_CtrAtt_IroHsd(void *data)
{
	if (data != NULL)
	{
		char buf_data[100] = "";
		sprintf((char *)buf_data, "[UP,iro_hsd,%d,%s]", num_filters, (char *)data);
		UART_sendToQueueToSend((char *)buf_data);
	}
	return 0;
}

uint8_t PROTO_CtrATT_IroRsft(void *data)
{
	if (data != NULL)
	{
		char buf_data[100] = "";
		sprintf((char *)buf_data, "[UP,iro_rsft,1,%s]", (char *)data);
		UART_sendToQueueToSend((char *)buf_data);
	}
	return 0;
}

uint8_t PROTO_CtrATT_ctrlDisp(void *data)
{
	if (data != NULL)
	{
		char buf_data[100] = "";
		sprintf((char *)buf_data, "[UP,ctrl_disp,2,%s]", (char *)data);
		UART_sendToQueueToSend((char *)buf_data);
	}
	return 0;
}
uint8_t PROTO_CtrATT_ctrlHot(void *data)
{
	if (data != NULL)
	{
		char buf_data[100] = "";
		sprintf((char *)buf_data, "[UP,ctrl_hot,1,%s]", (char *)data);
		UART_sendToQueueToSend((char *)buf_data);
	}
	return 0;
}
uint8_t PROTO_CtrATT_ctrlCold(void *data)
{
	if (data != NULL)
	{
		char buf_data[100] = "";
		sprintf((char *)buf_data, "[UP,ctrl_cold,1,%s]", (char *)data);
		UART_sendToQueueToSend((char *)buf_data);
	}
	return 0;
}
//xử lí dữ liệu nhận từ lênh UP từ uart của IRO

iro_struct_att_to_wifi_t att_struct[IRO_MAX_NUM_ATT] =
	{
		{IRO_VAN_XA, "iro_vx"},
		{IRO_BOM, "iro_bom"},
		{IRO_KET_NUOC, "iro_ketn"},
		{IRO_NUOC_VAO, "iro_nuocIn"},
		{IRO_lifeTime, "iro_lTime"},	  //		 timeExpireRemain
		{IRO_HSD, "iro_hsd"},			  //r,w    timeExpireTotal
		{IRO_RUN_TIME_TOTAL, "iro_rtt"},  //r,w   timeFilterTotal
		{IRO_RUN_TIME_REMAIN, "iro_rtr"}, //       timeFilterRemain
		{IRO_RESET_FILTER, "iro_rsft"},   //w
		{IRO_TDS, "iro_tds"},
		{IRO_pumpTime, "iro_bomTime"},
		{IRO_ERRS, "iro_errs"},
		{IRO_tempHot,   "temp_hot"},
		{IRO_tempCold,   "temp_cold"},
		{IRO_ctrlDisp,   "ctrl_disp"},
		{IRO_ctrlHot,   "ctrl_hot"},
		{IRO_ctrlCold,   "ctrl_cold"}};
//ex: //data_in: iro_rtr,9,300,300,300,3600,500,300,300,300,400
bool handleDataMsgFromUart(uint8_t *data_in, char *value_iro)
{
	uint8_t index = 0;
	bool isValid = false;
	uint8_t *ptr = NULL;

	//find property_code
	char *ptr_token = NULL;
	ptr_token = strtok((char *)data_in, ","); //ptr_token ->"iro_rtt"
	for (index = 0; index < IRO_MAX_NUM_ATT; index++)
	{
		if ((const char *)strcmp(att_struct[index].property_code, (const char *)ptr_token) == 0)
		{
			isValid = true;
			log_info("propertycode: %s", ptr_token);
			break;
		}
	}
	if (isValid == false)
	{
		log_info("error_Property_code");
		return isValid;
	}
	ptr = data_in + strlen(ptr_token) + 1; //ptr -->9
	ptr_token = strtok(NULL, ",");		   //ptr_token-> '9'
	uint16_t len = atoi(ptr_token);		   //len = 9
	log_info("num item: %d", len);
	if (len > 0)
		ptr = ptr + strlen(ptr_token) + 1; //ex: ptr -->"300....

	//data ok
	MQTT_SetPropertyCode((char *)att_struct[index].property_code); //property code
	switch (att_struct[index].att)
	{
	case IRO_VAN_XA:
		return false;
	case IRO_BOM:
		return false;
	case IRO_KET_NUOC:
		return false;
	case IRO_NUOC_VAO:
	case IRO_TDS:
	case IRO_tempHot:
	case IRO_tempCold:
	case IRO_ctrlHot:
	case IRO_ctrlCold:
	{
		if (len != 1)
		{
			log_info("error len");
			return false;
		}
		sprintf((char *)value_iro, "%s", (char *)ptr); //data:
	}
	break;
	case IRO_RESET_FILTER:
	{
		if (len != 1)
		{
			log_info("error len");
			return false;
		}
		sprintf((char *)value_iro, "[%s]", (char *)ptr);
		break;
	}
	case IRO_lifeTime:
	case IRO_RUN_TIME_REMAIN:
	case IRO_HSD:
	case IRO_RUN_TIME_TOTAL:
	case IRO_ctrlDisp:
	{
		sprintf((char *)value_iro, "[%s]", (char *)ptr); //data [300,300,300,3600,500,300,300,300,400]
		break;
	}
	case IRO_ERRS:
	{
		if (len == 0)
		{
			strcpy((char *)value_iro, "[]");
		}
		else
		{
			sprintf((char *)value_iro, "[%s]", (char *)ptr);
		}

		break;
	}
	default:
		isValid = false;
		break;
	}
	return isValid;
}

void PROTO_processUpdateProperty(cJSON *msgObject, const char *propertyCode)
{
	cJSON *info = NULL;
	char *mid = NULL, *reason = NULL;
	if (cJSON_HasObjectItem(msgObject, "i")) //kiem tra xem lenh gui xuong qua mqtt co ..
	{										 //.. chua thong tin gi khac khong: info,reason,mid
		info = cJSON_GetObjectItem(msgObject, "i");
	}
	if (cJSON_HasObjectItem(msgObject, "mid"))
	{
		mid = cJSON_GetObjectItem(msgObject, "mid")->valuestring;
	}
	if (cJSON_HasObjectItem(msgObject, "r"))
	{
		reason = cJSON_GetObjectItem(msgObject, "r")->valuestring;
	}
	else
		reason = "MQTT";

	UART_UpdatePublishInfo(mid, reason, info);
	cJSON *arr = NULL;
	char pdata[100] = "";
	char *value_iro = NULL;
	arr = cJSON_GetObjectItem(msgObject, "d");
	if (arr == NULL)
	{
		log_info("CJSON_GetObiectItem unsuccess!");
		return;
	}

	if (arr->type == cJSON_String)
	{
		value_iro = arr->valuestring;
		log_info("data string: %s", value_iro);
	}
	else if (arr->type == cJSON_Array)
	{
		int num_item = cJSON_GetArraySize(arr);
		num_filters = num_item;
		for (int i = 0; i < num_item; i++)
		{
			char sub_str[20] = "";
			cJSON *item = cJSON_GetArrayItem(arr, i);
			sprintf((char *)sub_str, "%s,", cJSON_Print(item));
			strcat((char *)pdata, (char *)sub_str);
			log_info("arr_index %d: %s", i, cJSON_Print(item));
		}
		//sprintf((char*)pdata,"%s", cJSON_Print(arr));
		value_iro = &pdata[0];
		pdata[strlen(pdata) - 1] = '\0'; //loai dau ,
	}
	else if (arr->type == cJSON_Number)
	{
		//log_info("data number:%s", cJSON_Print(arr));
		sprintf((char *)pdata, "%d", arr->valueint);
		value_iro = &pdata[0];

		log_info("data number:%s", value_iro);
	}
	else
	{
		cJSON_Delete(msgObject);
		return;
	}
	if (value_iro == NULL)
	{
		cJSON_Delete(msgObject);
		return;
	}
	for (int i = 0; i < PROTO_NUM_OF_IN_MSG; i++)
	{
		if (strcmp(propertyCode, mqttInMsgHandleTable[i].cmdType) == 0)
		{
			mqttInMsgHandleTable[i].msgHandle(value_iro); //goi ham gui  cho IRO tuong ung
		}
	}
}

/*tach du lieu nhan duoc mqtt server
*/
void PROTO_processCmd(char *data)
{
	cJSON *msgObject = cJSON_Parse(data);
	if (msgObject == NULL)
	{
		log_info("CJSON_Parse unsuccess!");
		return;
	}

	if (strcmp(topic_filter[EVENT_TYPE], EVT_CONTROL_PROPERTY) == 0) //event type: control property CP
	{
		cJSON *info = NULL;
		char *mid = NULL, *reason = NULL;
		if (cJSON_HasObjectItem(msgObject, "i")) //kiem tra xem lenh gui xuong qua mqtt co ..
		{										 //.. chua thong tin gi khac khong: info,reason,mid
			info = cJSON_GetObjectItem(msgObject, "i");
		}
		if (cJSON_HasObjectItem(msgObject, "mid"))
		{
			mid = cJSON_GetObjectItem(msgObject, "mid")->valuestring;
		}
		if (cJSON_HasObjectItem(msgObject, "r"))
		{
			reason = cJSON_GetObjectItem(msgObject, "r")->valuestring;
		}
		else
			reason = "MQTT";

		UART_UpdatePublishInfo(mid, reason, info);
		cJSON *arr = NULL;
		char pdata[100] = "";
		char *value_iro = NULL;
		arr = cJSON_GetObjectItem(msgObject, "d");
		if (arr == NULL)
		{
			log_info("CJSON_GetObiectItem unsuccess!");
			return;
		}

		if (arr->type == cJSON_String)
		{
			value_iro = arr->valuestring;
			log_info("data string: %s", value_iro);
		}
		else if (arr->type == cJSON_Array)
		{
			int num_item = cJSON_GetArraySize(arr);
			num_filters = num_item;
			for (int i = 0; i < num_item; i++)
			{
				char sub_str[20] = "";
				cJSON *item = cJSON_GetArrayItem(arr, i);
				sprintf((char *)sub_str, "%s,", cJSON_Print(item));
				strcat((char *)pdata, (char *)sub_str);
				log_info("arr_index %d: %s", i, cJSON_Print(item));
			}
			//sprintf((char*)pdata,"%s", cJSON_Print(arr));
			value_iro = &pdata[0];
			pdata[strlen(pdata) - 1] = '\0'; //loai dau ,
		}
		else if (arr->type == cJSON_Number)
		{
			//log_info("data number:%s", cJSON_Print(arr));
			sprintf((char *)pdata, "%d", arr->valueint);
			value_iro = &pdata[0];

			log_info("data number:%s", value_iro);
		}
		else
		{
			cJSON_Delete(msgObject);
			return;
		}
		if (value_iro == NULL)
		{
			cJSON_Delete(msgObject);
			return;
		}
		for (int i = 0; i < PROTO_NUM_OF_IN_MSG; i++)
		{
			if (strcmp(topic_filter[PROPERTY_CODE], mqttInMsgHandleTable[i].cmdType) == 0)
			{
				mqttInMsgHandleTable[i].msgHandle(value_iro); //goi ham gui  cho IRO tuong ung
			}
		}
	}
	else if (strcmp(topic_filter[EVENT_TYPE], EVT_UPDATE_FIRMWARE) == 0) //event type: update firmware UF
	{
		// linkDownLoad[0] = '\0';
		// strcpy(linkDownLoad, cJSON_GetObjectItem(msgObject, "link")->valuestring);
		char *linkFile = cJSON_GetObjectItem(msgObject, "link")->valuestring;
		// log_info("link file: %s", linkDownLoad);
		if (cJSON_HasObjectItem(msgObject, "type"))
		{
			int type = cJSON_GetObjectItem(msgObject, "type")->valueint;
			if(type == 0)
			{
				OTA_http_DoOTA(linkFile, PHASE_OTA_ESP);
			}
			else if(type == 1)
			{
				if (cJSON_HasObjectItem(msgObject, "mcuHardVer"))
				{
					int ver = cJSON_GetObjectItem(msgObject, "mcuHardVer")->valueint;
					mcuHardVer = ver;
				}

				OTA_http_DoOTA(linkFile, PHASE_OTA_MCU);
			}
			else if(type == 2)
			{
				OTA_http_DoOTA(linkFile, PHASE_OTA_MCU_VOICE);
			}
		}
		else{
			OTA_http_DoOTA(linkFile, PHASE_OTA_ESP);
		}
		
	}
	else if (strcmp(topic_filter[EVENT_TYPE], EVT_SCHEDULE_CHANGE) == 0) //event type: schedule change SC
	{

	}

	cJSON_Delete(msgObject);
}

mqtt_certKey_t *pMqttCertKey = NULL;
uint16_t lenCert = 0;
uint16_t lenKey = 0;
extern char g_product_Id[PRODUCT_ID_LEN];
extern bool g_mqttHaveNewCertificate;
extern bool g_enTest;
void PROTO_processCertificate(char* topic, char* data)
{
	char* part1 = NULL, *part2 = NULL, *part3 = NULL;
	part1 = strtok(topic, "/");if(part1 == NULL) return;
	part2 = strtok(NULL, "/");if(part2 == NULL) return;
	part3 = strtok(NULL, "/");if(part3 == NULL) return;
#ifdef AWS_PHASE_PRODUCTION
	printf("%s %s %s\n", part1, part2, part3);
#elif defined(AWS_PHASE_DEVERLOP) || defined(AWS_PHASE_STAGING)
	char *part4 = NULL;
	part4 = strtok(NULL, "/");if(part4 == NULL) return;
	printf("%s %s %s %s\n", part1, part2, part3, part4);
#endif
	//can sua lai theo fomat moi co filter product Id
#ifdef AWS_PHASE_PRODUCTION
	if(strcmp(part3, g_product_Id) != 0) return;
#elif defined(AWS_PHASE_DEVERLOP) || defined(AWS_PHASE_STAGING)
	if(strcmp(part4, g_product_Id) != 0) return;
#endif

#ifdef AWS_PHASE_PRODUCTION
	if(strcmp(part2, "getcert") == 0)
#elif defined(AWS_PHASE_DEVERLOP) || defined(AWS_PHASE_STAGING)
	if(strcmp(part3, "getcert") == 0)
#endif
	{
		cJSON *msgObject = cJSON_Parse(data);
		if(msgObject == NULL) return;
		if(cJSON_HasObjectItem(msgObject, "status"))
		{
			int status = cJSON_GetObjectItem(msgObject, "status")->valueint;
			printf("status get cert: %d\n", status);
			if(status == 1)
			{
				printf("only get cert one time for code test\n");
				BLE_releaseBle();
				if(cJSON_HasObjectItem(msgObject, "payload"))
				{
					cJSON *payload = cJSON_GetObjectItem(msgObject, "payload");
					if(cJSON_HasObjectItem(payload, "link"))
					{
						cJSON *link = cJSON_GetObjectItem(payload, "link");
						if(link->type == cJSON_Array)
						{
							if(cJSON_GetArraySize(link) == 2)
							{
								char* link1 = cJSON_GetStringValue(cJSON_GetArrayItem(link, 0));
								char* link2 = cJSON_GetStringValue(cJSON_GetArrayItem(link, 1));
								char* linkCert = NULL; char* linkKey = NULL;
								if(strstr(link1, ".crt") != NULL && strstr(link2, ".key") != NULL)
								{
									linkCert = link1;
									linkKey = link2;
								}
								else if(strstr(link1, ".key") != NULL && strstr(link2, ".crt") != NULL){
									linkCert = link2;
									linkKey = link1;
								}
								if(linkCert != NULL && linkKey != NULL && pMqttCertKey == NULL)
								{
									pMqttCertKey = (mqtt_certKey_t*)calloc(1,sizeof(mqtt_certKey_t));
									if(pMqttCertKey != NULL)
									{	
										lenCert = 0;
										lenKey = 0;
										printf("link cert: %s\n",linkCert);
										if(Http_downloadFileCertificate(linkCert) == true)
										{
											printf("%s",pMqttCertKey->cert);
										// log_info("Free heap: %d bytes\n", xPortGetFreeHeapSize());
											printf("link key: %s\n", linkKey);

											if(Http_downloadFileKeyMqtt(linkKey) == true)
											{
												printf("%s",pMqttCertKey->key);
												printf("%d %d\n",lenCert, lenKey);
												MqttHandle_startCheckNewCerificate();
											}
										}

										// free(pMqttCertKey);
									}
									else{
										UART_sendToQueueToSend("[RQS_CERTIFI,1,]");
										//rsp jig
									}
								}
								else{
									UART_sendToQueueToSend("[RQS_CERTIFI,1,]");
								}
							}
							else{
								UART_sendToQueueToSend("[RQS_CERTIFI,1,]");
								//rsp jig
							}
						}
						else{
							UART_sendToQueueToSend("[RQS_CERTIFI,1,]");
						}
					}
					else{
						UART_sendToQueueToSend("[RQS_CERTIFI,1,]");
						//rsp jig
					}
				}
				else{
					UART_sendToQueueToSend("[RQS_CERTIFI,1,]");
					//rsp jig
				}
			}
			else{
				//rsp jig
				UART_sendToQueueToSend("[RQS_CERTIFI,1,]");
			}
		}
		cJSON_Delete(msgObject);
	}
#ifdef AWS_PHASE_PRODUCTION
	else if(strcmp(part2, "confirm") == 0)
#elif defined(AWS_PHASE_DEVERLOP) || defined(AWS_PHASE_STAGING)
	else if(strcmp(part3, "confirm") == 0)
#endif
	{

		cJSON *msgObject = cJSON_Parse(data);
		if(msgObject == NULL) return;
		if(cJSON_HasObjectItem(msgObject, "status"))
		{
			int status = cJSON_GetObjectItem(msgObject, "status")->valueint;
			printf("status confirm: %d\n", status);
			if(status == 1)
			{
				if(cJSON_HasObjectItem(msgObject, "payload"))
				{
					cJSON *payload = cJSON_GetObjectItem(msgObject, "payload");
					if(cJSON_HasObjectItem(payload,"message"))
					{
						if(strcmp("OK",cJSON_GetObjectItem(payload, "message")->valuestring) == 0)
						{
							//ok
							printf("confirm ok\n");
							if(FlashHandler_saveCertKeyMqttInStore() == true)
							{
								FlashHandler_saveEnvironmentMqttInStore();
								//send ok jig
								g_mqttHaveNewCertificate = true;
								UART_sendToQueueToSend("[RQS_CERTIFI,0,]"); 
								if(!g_enTest)
                                {
									printf("restart esp after 1s\n");
                                    vTaskDelay(1000/portTICK_PERIOD_MS);
                                    esp_restart();
                                }

							}
						}
						else{
							UART_sendToQueueToSend("[RQS_CERTIFI,1,]");
						}
					}
					else{
						UART_sendToQueueToSend("[RQS_CERTIFI,1,]");
					}
				}
				else{
					UART_sendToQueueToSend("[RQS_CERTIFI,1,]");
				}
			}
			else{
				UART_sendToQueueToSend("[RQS_CERTIFI,1,]");
			}
		}
		cJSON_Delete(msgObject);
	}
	else return;
}