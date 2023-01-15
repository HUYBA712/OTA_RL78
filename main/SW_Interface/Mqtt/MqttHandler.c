#include "MqttHandler.h"
#include "Wifi_handler.h"
#include "HTG_Utility.h"
#include "Gateway_config.h"
#include "MKP_W_Protocol.h"
#include "OutputControl.h"
#include "HttpHandler.h"
#include "UART_Handler.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "DateTime.h"
#include "mqtt_client.h"
#include "UartVoice.h"
#include "BLE_handler.h"
#include "FlashHandle.h"
#include "esp32/rom/rtc.h"
#ifndef DISABLE_LOG_ALL
#define MQTT_TIME_LOG_INFO_ON
#endif


#ifdef MQTT_TIME_LOG_INFO_ON
#define log_info(format, ... ) ESP_LOGI("MQTT_Handler",  format , ##__VA_ARGS__)
#else
#define log_info(format, ... )
#endif

#define MAX_LEN_MSG 250
#define MQTT_PORT 1883
//--------------------------------------- for MQTT connect
#define DEFAULT_PWD "mkpsmarthome"
extern char g_product_Id[PRODUCT_ID_LEN]; //WIFI MAC Address ESP32
extern char g_password[PASSWORD_LEN];
char topic_filter[4][30] = {0};   //
char g_mqttHost[50] = MQTT_SERVER;


extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");

extern const uint8_t mqtt_eclipse_org_pem_start[]   asm("_binary_mqtt_eclipse_org_pem_start");
extern const uint8_t mqtt_eclipse_org_pem_end[]   asm("_binary_mqtt_eclipse_org_pem_end");


mqtt_certKey_t certKeyMqtt = {"none", "none"};
mqtt_environment_t envir = {"none"};
//-------------------------------------------------------
esp_mqtt_client_handle_t g_mqttCientHandle;
esp_mqtt_client_handle_t g_mqttCientHandle_mkp;
bool g_isMqttConnected = false;
bool g_isMqttConnected_mkp = false;
bool g_mqttHaveNewCertificate = false;
bool needConfirmNewCerificate = false;
//------------------------------------------------------
//Khai bao nguyen mau ham-------------------
void topic_name_filter(char *data, int len);
void sub_string(char *des,char *src,int start, int end);

//------------------------------------------
void MQTT_SubscribeToDeviceTopic(char *product_Id)
{
	if(!g_mqttHaveNewCertificate) return;
	char topicName[50] = "";
	subTopicFromProductId(product_Id, topicName);
	int msg_id = esp_mqtt_client_subscribe(g_mqttCientHandle, topicName, 0);
    log_info(" Subcribe to topic: %s , id: %d", topicName, msg_id);

}
int MQTT_SubscribeTopicGetCert()
{
	char ptopic[100] = "";
	#ifdef AWS_PHASE_STAGING
	sprintf(ptopic, "certificates/stg/getcert/%s", g_product_Id);
	#elif defined(AWS_PHASE_DEVERLOP)
	sprintf(ptopic, "certificates/dev/getcert/%s", g_product_Id);
	#elif  defined(AWS_PHASE_PRODUCTION)
	sprintf(ptopic, "certificates/getcert/%s", g_product_Id);
	#endif
	int msg_id = esp_mqtt_client_subscribe(g_mqttCientHandle, ptopic, 0);
    log_info(" Subcribe to topic: %s , id: %d", ptopic, msg_id);
	return msg_id;
}
int MQTT_SubscribeTopicConfirmCert()
{
	char ptopic[100] = "";
	#ifdef AWS_PHASE_STAGING
	sprintf(ptopic, "certificates/stg/confirm/%s", g_product_Id);
	#elif defined(AWS_PHASE_DEVERLOP)
	sprintf(ptopic, "certificates/dev/confirm/%s", g_product_Id);
	#elif  defined(AWS_PHASE_PRODUCTION)
	sprintf(ptopic, "certificates/confirm/%s", g_product_Id);
	#endif
	int msg_id = esp_mqtt_client_subscribe(g_mqttCientHandle, ptopic, 0);
    log_info(" Subcribe to topic: %s , id: %d", ptopic, msg_id);
	return msg_id;
}
extern bool g_enTest;
extern bool g_registerOk;
static void connectedTask(void *arg)
{ 
	if(!g_enTest){
		vTaskDelay(500 / portTICK_RATE_MS);
		Wifi_checkRssi();
		UART_needUpdateAllParam();
		UartVoice_updateFirmVer();
		if(!g_mqttHaveNewCertificate)
		{
	        MQTT_PublishRequestGetCeritificate(g_product_Id);
		}
	}
	// if(Http_sendFirmWareVer() == true)
	// {
	// 	g_registerOk = true;
	// 	if(g_enTest)
	// 	{
	// 		//UART_sendToQueueToSend("[REG_STT,0,]");
	// 	}
	// }
	// else{
	// 	if(g_enTest)
	// 	{
	// 		//UART_sendToQueueToSend("[REG_STT,1,]");
	// 	}
	// }
    vTaskDelete(NULL);
}
/*
Note: If message is too long, there will be multiple event
For now, message too long is not handled
*/
void MQTT_data_cb(esp_mqtt_event_handle_t event)
{
	char *topic = (char *)malloc(event->topic_len + 1);
	memcpy(topic, event->topic, event->topic_len);
	topic[event->topic_len] = 0;
	log_info("[Receive] topic: %s", topic);

	char *data = (char *)malloc(event->data_len + 1);
	memcpy(data, event->data + event->current_data_offset, event->data_len);
	data[event->data_len] = 0;
	log_info("[Receive] data[%d/%d bytes]:\n%s",
			 event->data_len + event->current_data_offset,
			 event->total_data_len, data);

	if(strncmp(topic,"certificates",12) == 0)
	{	
		PROTO_processCertificate(topic, data);
	}
	else{
		topic_name_filter(topic, event->topic_len);

		log_info("Username: %s\n", topic_filter[USER_NAME]);
		log_info("EventType: %s\n", topic_filter[EVENT_TYPE]);
		log_info("LocalID: %s\n", topic_filter[LOCAL_ID]);
		log_info("PropertyCode: %s\n", topic_filter[PROPERTY_CODE]);

		PROTO_processCmd(data);
	}
	

	free(topic);
	free(data);
}
bool s_isFirstConnected = true;
int msgIdSubConfirm = -1;
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
	// your_context_t *context = event->context;
	switch (event->event_id)
	{
	case MQTT_EVENT_CONNECTED:
		log_info(" MQTT_EVENT_CONNECTED");
		MQTT_SubscribeToDeviceTopic(g_product_Id);
		if(!g_mqttHaveNewCertificate)
			MQTT_SubscribeTopicGetCert();
		if(needConfirmNewCerificate)
			msgIdSubConfirm = MQTT_SubscribeTopicConfirmCert();
		g_isMqttConnected = true;
		if (s_isFirstConnected)
		{
			xTaskCreate(connectedTask, "connectedTask", 3 * 1024, NULL, 1, NULL);
			s_isFirstConnected = false;
		}
		else{
			UART_needUpdateAllParam();
		}
		break;
	case MQTT_EVENT_DISCONNECTED:
		log_info("MQTT_EVENT_DISCONNECTED");
		g_isMqttConnected = false;
		break;

	case MQTT_EVENT_SUBSCRIBED:
		log_info("MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		if(event->msg_id == msgIdSubConfirm)
		{
			if(needConfirmNewCerificate)
			{
				MQTT_PublishRequestConfirmCeritificate(g_product_Id);
				needConfirmNewCerificate = false;
			}
		}	
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		log_info("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_PUBLISHED:
		log_info("MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_DATA:
		log_info("MQTT_EVENT_DATA");
		MQTT_data_cb(event);
		break;
	case MQTT_EVENT_ERROR:
		log_info("MQTT_EVENT_ERROR");
		break;
	default:
		log_info("Other event id:%d", event->event_id);
		break;
	}
	return ESP_OK;
}
static esp_err_t mqtt_event_handler_mkp(esp_mqtt_event_handle_t event)
{
    // your_context_t *context = event->context;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        log_info(" MQTT_EVENT_CONNECTED [MKP]");
        char topicName[50] = "";
        subTopicFromProductId(g_product_Id, topicName);
        esp_mqtt_client_subscribe(g_mqttCientHandle_mkp, topicName, 0);
        log_info(" Subcribe to topic [MKP]: %s", topicName);
        g_isMqttConnected_mkp = true;
        if(!g_enTest){
            MQTT_PublishReasonReset();
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        log_info("MQTT_EVENT_DISCONNECTED [MKP]");
        g_isMqttConnected_mkp = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        log_info("MQTT_EVENT_SUBSCRIBED [MKP], msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        log_info("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        log_info("MQTT_EVENT_PUBLISHED [MKP], msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        log_info("MQTT_EVENT_DATA [MKP]");
        MQTT_data_cb(event);
        break;
    case MQTT_EVENT_ERROR:
        log_info("MQTT_EVENT_ERROR");
        break;
    default:
        log_info("Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}
void MQTT_Init()
{
	if(FlashHandler_getEnvironmentMqttInStore() == false)
    {
        FlashHandler_saveEnvironmentMqttInStore();
    }
	if(FlashHandler_getCertKeyMqttInStore() == false)
	{
		// FlashHandler_saveCertKeyMqttInStore();
	}
	else
	{
		if(strcmp(certKeyMqtt.cert,"none") == 0 && strcmp(certKeyMqtt.key,"none") == 0)
		{
			g_mqttHaveNewCertificate = false;
		}
		else{
			g_mqttHaveNewCertificate = true;
			if(strcmp(envir.environment, ENVIR_STR) != 0)    //sai envir hoac envir moi
            {
                g_mqttHaveNewCertificate = false;
                printf("had cert but bad cert\n");
            }
		}
	}
	printf("cer is have new cer: %s, len cer: %d len key: %d\n",g_mqttHaveNewCertificate == true ? "yes":"no", strlen(certKeyMqtt.cert), strlen(certKeyMqtt.key));
	if(!g_mqttHaveNewCertificate)
	{
		strcpy(envir.environment, ENVIR_STR);
		strcpy(certKeyMqtt.cert, (const char*)client_cert_pem_start);
		strcpy(certKeyMqtt.key, (const char*)client_key_pem_start);
		printf("copy cert key default: %d, %d\n", strlen(certKeyMqtt.cert), strlen(certKeyMqtt.key));
	}
	
	// if(strcmp(certKeyMqtt.cert,"none") != 0 && strcmp(certKeyMqtt.key,"none") != 0)
	// {
		const esp_mqtt_client_config_t mqtt_cfg = {
		#ifdef AWS_PHASE_PRODUCTION
			.uri = "mqtts://a1c5v69u5m9t9z-ats.iot.ap-southeast-1.amazonaws.com:8883",
        #else
            .uri = "mqtts://atun55ecc1yka-ats.iot.ap-southeast-1.amazonaws.com:8883",
        #endif
        .event_handle = mqtt_event_handler,
        .client_cert_pem = (const char *)(certKeyMqtt.cert),
        .client_key_pem = (const char *)(certKeyMqtt.key),
        .cert_pem = (const char *)mqtt_eclipse_org_pem_start,
		.client_id = g_product_Id,
    	};
		g_mqttCientHandle = esp_mqtt_client_init(&mqtt_cfg);
	// }
	// else{
	// 	 const esp_mqtt_client_config_t mqtt_cfg = {
    //     .uri = "mqtts://atun55ecc1yka-ats.iot.ap-southeast-1.amazonaws.com:8883",
    //     .event_handle = mqtt_event_handler,
    //     .client_cert_pem = (const char *)client_cert_pem_start,
    //     .client_key_pem = (const char *)client_key_pem_start,
    //     .cert_pem = (const char *)mqtt_eclipse_org_pem_start,
	// 	.client_id = g_product_Id,
    // 	};
	// 	g_mqttCientHandle = esp_mqtt_client_init(&mqtt_cfg);
	// }
    // ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    // esp_mqtt_client_start(g_mqttCientHandle);
#ifndef DISABLE_SERVER_MKP
    // g_mqttHaveNewCertificate = true;
    esp_mqtt_client_config_t mqtt_cfg1 = {
        .host = g_mqttHost,
        .event_handle = mqtt_event_handler_mkp,
        .username = g_product_Id,
        .password = g_password,
        .client_id = g_product_Id,
        .port = MQTT_PORT,
        .keepalive = 60,
    };
    g_mqttCientHandle_mkp = esp_mqtt_client_init(&mqtt_cfg1);
#endif
}
//Note: update mqtt config when start
void MQTT_Start()
{
	if (!g_isMqttConnected)
	{
		// esp_mqtt_client_config_t mqtt_cfg = {
		// 	.uri = "mqtts://atun55ecc1yka-ats.iot.ap-southeast-1.amazonaws.com:8883",
		// 	.event_handle = mqtt_event_handler,
		// 	.client_cert_pem = (const char *)client_cert_pem_start,
		// 	.client_key_pem = (const char *)client_key_pem_start,
		// 	.cert_pem = (const char *)mqtt_eclipse_org_pem_start,
		// };
		// esp_mqtt_set_config(g_mqttCientHandle, &mqtt_cfg);
		esp_mqtt_client_start(g_mqttCientHandle);
	}
#ifndef DISABLE_SERVER_MKP
    if (!g_isMqttConnected_mkp)
    {
        esp_mqtt_client_config_t mqtt_cfg1 = {
            .host = g_mqttHost,
            .event_handle = mqtt_event_handler_mkp,
            .username = g_product_Id,
            .password = g_password,
            .client_id = g_product_Id,
            .port = MQTT_PORT,
            .keepalive = 60,
            .disable_clean_session = false
        };
        esp_mqtt_set_config(g_mqttCientHandle_mkp,&mqtt_cfg1);
        esp_mqtt_client_start(g_mqttCientHandle_mkp);
    }
#endif
}
void MQTT_Stop()
{
	if (g_isMqttConnected)
	{
		esp_mqtt_client_stop(g_mqttCientHandle);
	}
#ifndef DISABLE_SERVER_MKP
    if (g_isMqttConnected_mkp)
    {
        esp_mqtt_client_stop(g_mqttCientHandle_mkp);
    }
#endif
}
//--------------------------------------------------------------------------------------------- out message
void MQTT_PublishToThisDeviceTopic(char *pubData){
	MQTT_PublishToDeviceTopic(g_product_Id, pubData);
}
int MQTT_PublishToServerMkp(char* pubTopicName,char* pubData)
{
    if (!g_isMqttConnected_mkp)
        return -1;
    if(esp_mqtt_client_publish(g_mqttCientHandle_mkp, pubTopicName, pubData, strlen(pubData), 0, 0) == -1)
    {
        return -1;
    }
    log_info(" [Device] Publish Topic [mkp]: %s",pubTopicName);
    log_info(" [Device] Publish Data [mkp]: %s",pubData);
    return 0;
}
int MQTT_PublishToDeviceTopic(char* pubTopicName,char* pubData)
{
	if(!g_mqttHaveNewCertificate)
	{
		return 0;
	}
	if (!g_isMqttConnected)
		return -1;
	if(esp_mqtt_client_publish(g_mqttCientHandle, pubTopicName, pubData, strlen(pubData), 0, 0) == -1)
	{
		return -1;
	}
	log_info(" [Device] Publish Topic: %s",pubTopicName);
	log_info(" [Device] Publish Data: %s",pubData);
	//pubTopicFromProductId(productId, topicName);
	//mqtt_publish(client, topicName, pubData, strlen(pubData), 0, 0);
	return 0;
}
void MQTT_PublishReasonReset()
{
    if(!g_isMqttConnected_mkp)
    {
        return;
    }
    char pData[30] = "[]";
    char pubTopicName[100] = {0};
    sprintf((char*)pData,"{\"d\":%d}",rtc_get_reset_reason(0));

    pubTopicFromProductId(g_product_Id,"UP","0","reasonReset",pubTopicName); 
    MQTT_PublishToServerMkp(pubTopicName,pData);                                     
}
void MQTT_PublishVersion(uint16_t hardver, uint16_t firmver, uint16_t mcuFirmver, uint16_t mcuHardver)
{
    if(!g_mqttHaveNewCertificate)
    {
        return;
    }
    char pData[MAX_LEN_MSG] = "[]";
    char pubTopicName[100] = {0};
    sprintf((char*)pData,"{\"d\":{\"seriNumber\":\"%s\",\"hardwareVerOfFirm\":\"%d\",\"firmwareVersion\":\"%d\",\"mcuFirmVer\":\"%d\",\"mcuHardVer\":\"%d\"}}",g_product_Id,hardver, firmver, mcuFirmver,mcuHardver);

    pubTopicFromProductId(g_product_Id,"UP","1","f_version",pubTopicName); 
    MQTT_PublishToDeviceTopic(pubTopicName,pData); 
    MQTT_PublishToServerMkp(pubTopicName,pData);                                    
}

void MQTT_PublishRssi(int8_t newRssi)
{
	if(!g_mqttHaveNewCertificate)
	{
		return;
	}
    char pData[MAX_LEN_MSG] = "[]";
    char pubTopicName[100] = {0};
    sprintf((char*)pData,"{\"d\":%d}",newRssi);

    pubTopicFromProductId(g_product_Id,"UP","0","rssi",pubTopicName); 
    MQTT_PublishToDeviceTopic(pubTopicName,pData);   
	MQTT_PublishToServerMkp(pubTopicName,pData);                                   
}

void MQTT_PublishDataCommon(char* data, char* property)
{
	char pData[MAX_LEN_MSG] = "[]";
    char pubTopicName[100] = {0};
    sprintf((char*)pData,"{\"d\":\"%s\"}",data);

    pubTopicFromProductId(g_product_Id,"UP","1",property,pubTopicName); 
    MQTT_PublishToDeviceTopic(pubTopicName,pData);     
}
int MQTT_PublishValueIro(char *localId, char* value_iro, char *mid, char *reason, char *info)
{
	if(!g_mqttHaveNewCertificate)
	{
		return 0;
	}
	if (!g_isMqttConnected)
		return -1;
    char pData[MAX_LEN_MSG] = "[]";
    char pubTopicName[100] = {0};
	//cJSON *pData_info = NULL;
	if(reason == NULL)
	{
    	sprintf((char*)pData,"{\"d\":%s}",value_iro);
	}
	pubTopicFromProductId(g_product_Id,"UP",localId,topic_filter[PROPERTY_CODE],pubTopicName); 
	MQTT_PublishToDeviceTopic(pubTopicName,pData);										
	printf("\r\n");
	return 0;
}
int MQTT_PublishRequestGetCeritificate(char* productId)
{
	char pData[100] = "";
	char pTopic[100] = "";
	#ifdef AWS_PHASE_STAGING
	strcpy((char*)pTopic, "certificates/stg/getcert");
	#elif defined(AWS_PHASE_DEVERLOP)
	strcpy((char*)pTopic, "certificates/dev/getcert");
	#elif  defined(AWS_PHASE_PRODUCTION)
	strcpy((char*)pTopic, "certificates/getcert");
	#endif
	sprintf((char*)pData,"{\"serial_number\":\"%s\"}",productId);
	if (!g_isMqttConnected)
		return -1;
	if(esp_mqtt_client_publish(g_mqttCientHandle, pTopic, pData, strlen(pData), 0, 0) == -1)
	{
		return -1;
	}
	log_info(" [Device] Publish Topic: %s",pTopic);
	log_info(" [Device] Publish Data: %s",pData);
	return 0;
}
int MQTT_PublishRequestConfirmCeritificate(char* productId)
{
	char pData[100] = "";
	char pTopic[100] = "";
	#ifdef AWS_PHASE_STAGING
	strcpy((char*)pTopic, "certificates/stg/confirm");
	#elif defined(AWS_PHASE_DEVERLOP)
	strcpy((char*)pTopic, "certificates/dev/confirm");
	#elif  defined(AWS_PHASE_PRODUCTION)
	strcpy((char*)pTopic, "certificates/confirm");
	#endif
	sprintf((char*)pData,"{\"serial_number\":\"%s\"}",productId);
	if (!g_isMqttConnected)
		return -1;
	if(esp_mqtt_client_publish(g_mqttCientHandle, pTopic, pData, strlen(pData), 0, 0) == -1)
	{
		return -1;
	}
	log_info(" [Device] Publish Topic: %s",pTopic);
	log_info(" [Device] Publish Data: %s",pData);
	return 0;
}
void sub_string(char *des,char *src,int start, int end)
{
    int index = 0;
    for(int i = start + 1; i < end; i++)
    {
        des[index++] = src[i];
    }
    des[index] = '\0';
}
void topic_name_filter(char *data, int len)
{
    // format topic: d/proId/s/username/event_type/localID/propertycode
	// format ota topic: d/proId/s/admin/UF
    int index[7], count = 0, count_sub = 0;
    for(int i = 0; i < len; i++)
    {
        if(data[i] == '/')
        {
            index[count++] = i;
        }
    }
    index[count] = len; //cuoi cua topic name khong co dau "/" nen phai them chi so vao cuoi

    for(int i = 2; i < count; i++)
    {
        sub_string(topic_filter[count_sub++],data,index[i],index[i+1]);
    }
    
}
void MQTT_SetPropertyCode(char *propertyCode)
{
	strcpy(topic_filter[PROPERTY_CODE] , propertyCode);
}

extern mqtt_certKey_t *pMqttCertKey;
static void taskCheckFileCertificate(void* arg)
{
	while(1)
	{
		if(pMqttCertKey != NULL)
		{
			if(strlen(pMqttCertKey->key) > 0 && strlen(pMqttCertKey->cert) > 0)
			{
				esp_mqtt_client_disconnect(g_mqttCientHandle); //-> mqtt ->force to state timeout then auto reconnect
				printf("update config mqtt...\n");
				strcpy(certKeyMqtt.cert, (const char*)pMqttCertKey->cert);
				strcpy(certKeyMqtt.key, (const char*)pMqttCertKey->key);
				//update config
				// const esp_mqtt_client_config_t mqtt_cfg = {
				// .uri = "mqtts://atun55ecc1yka-ats.iot.ap-southeast-1.amazonaws.com:8883",
				// .event_handle = mqtt_event_handler,
				// .client_cert_pem = (const char *)(certKeyMqtt.cert),
				// .client_key_pem = (const char *)(certKeyMqtt.key),
				// .cert_pem = (const char *)mqtt_eclipse_org_pem_start,
				// .client_id = g_product_Id,
				// };
				// esp_mqtt_set_config(g_mqttCientHandle,&mqtt_cfg);
				printf("start reconnect mqtt...\n");//should call reconnect (force reconnect)  from disconnect
				esp_mqtt_client_reconnect(g_mqttCientHandle);
				needConfirmNewCerificate = true;
				break;
			}
			else{
				break;
			}
		}
		else{
			break;
		}
	}
	if(pMqttCertKey != NULL)
	{
		free(pMqttCertKey);
		pMqttCertKey = NULL;
	}
	vTaskDelete(NULL);
}
void MqttHandle_startCheckNewCerificate()
{
	xTaskCreate(taskCheckFileCertificate, "taskFileCertificate", 4096, NULL, 5, NULL);
}