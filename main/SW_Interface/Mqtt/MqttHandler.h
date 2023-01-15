#ifndef MQTT_HANDLER
#define MQTT_HANDLER 
#include "Global.h"
#include "cJSON.h"

#define AWS_PHASE_PRODUCTION
#define ENVIR_STR    "PROD"

// #define AWS_PHASE_DEVERLOP
// #define ENVIR_STR    "DEV"

// #define AWS_PHASE_STAGING
// #define ENVIR_STR    "STG"

enum s_topic_filter
{
    USER_NAME ,
    EVENT_TYPE,
    LOCAL_ID,
    PROPERTY_CODE
};


typedef struct 
{
    char cert[1500];
    char key[2000];
} mqtt_certKey_t;
typedef struct 
{
    char environment[10];
} mqtt_environment_t;


void MQTT_Init();
void MQTT_Start();
void MQTT_Stop();

void MQTT_SubscribeToDeviceTopic(char* product_Id);
int MQTT_PublishRequestGetCeritificate(char* productId);
int MQTT_PublishRequestConfirmCeritificate(char* productId);
bool FlashHandler_getCertKeyMqttInStore();
bool FlashHandler_saveCertKeyMqttInStore();
void MqttHandle_startCheckNewCerificate();

void MQTT_PublishReasonReset();
int MQTT_PublishToServerMkp(char* pubTopicName,char* pubData);
int MQTT_PublishToDeviceTopic(char *pubTopicName,char *pubData);
void MQTT_PublishToThisDeviceTopic(char *pubData);
int MQTT_PublishValueIro(char *localId, char* value_iro, char *mid, char *reason, char *info);
void MQTT_PublishDataCommon(char* data, char* property);
void MQTT_SetPropertyCode(char *propertyCode);
void MQTT_PublishRssi(int8_t newRssi);
void MQTT_PublishVersion(uint16_t hardver, uint16_t firmver, uint16_t mcuFirmver, uint16_t mcuHardver);
#endif
