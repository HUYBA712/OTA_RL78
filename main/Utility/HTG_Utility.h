#ifndef HTG_Utility_h
#define HTG_Utility_h

#include <stdio.h>
#include <stdlib.h>
#include "esp_system.h"

#define WIFI_SEPARATE '+'

int convertCharArrayToNumber(unsigned char length, char* input);

/*
Return true if *str start with *pre
*/
bool startsWith(const char *str, const char *pre);

/*
Get subtring from source string
return false if parent string is invalid
*/
bool subString(char *child, const char *parent, uint8_t begin, uint8_t length);

const char* Utils_errorToString(esp_err_t errCode);

bool getWifiFromBLEMes(char* ssid, char* pwd, char* dataStr);

uint8_t DecodeCheckSum(uint8_t high, uint8_t low);
void EncodeCheckSum(uint8_t checksum, char *high, char *low);
uint8_t PROTO_CalculateChecksum(uint8_t *buf, uint32_t len);

uint8_t getDeviceIdFromTopic(char *deviceId, char* topic);
uint64_t at64i(char *dataStr);
uint64_t valueFromHexString(char* hexStr);
void hexStringFromValue(char* hexStr, uint64_t value);
uint16_t convParamStringToNumber(uint8_t* dataString_in, uint16_t* dataNumber_out);
//----------------------------------MQTT
void pubTopicFromProductId(char* product_Id, char *eventType, char *localId, char *propertyCode, char* topicName);
void subTopicFromProductId(char* product_Id,char* topicName);
bool Buffer_add(uint8_t *buff,uint16_t *buffLen,uint8_t *newData,uint16_t newDataLen);

//Example:
    // char testStr[100] = "abcd:123:acse:123";
    // char* out[6];
    // uint8_t num = convStringToParam(testStr,(char **)out,":");
    // printf("num: %d ",num);
    // for (uint8_t i = 0; i < num; i++)
    // {
    //     printf(out[i]);
    //     printf("\n");
    // }

uint16_t convStringToParam(char* dataString_in, char** string_out,char* divStr);

#endif