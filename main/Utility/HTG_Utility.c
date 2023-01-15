#include "HTG_Utility.h"
#include <esp_err.h>
#include <nvs.h>
#include <esp_wifi.h>
#include <math.h>       
#include <string.h>
#include "stdlib.h"

int convertCharArrayToNumber(unsigned char length, char* input) {
	int output = 0;
	for (int i = 0; i < length; i++)
	{
		if ((input[i] >= '0') && (input[i] <= '9'))
		{
			output = output * 10 + input[i] - '0';
		}
	}
	return output;
}

bool startsWith(const char *str, const char *pre)
{
	size_t lenpre = strlen(pre),
	       lenstr = strlen(str);
	return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

bool subString(char *child, const char *parent, uint8_t begin, uint8_t length) {
	if(length ==0){
		length = strlen(parent) - begin;
	}
	if ((begin + length) > strlen(parent))
	{
		return false;
	}
	strncpy(child, parent + begin, length);
	child[length] = '\0';
	return true;
}

const char* Utils_errorToString(esp_err_t errCode) {
	switch (errCode) {
	case ESP_OK:
		return "OK";
	case ESP_FAIL:
		return "Fail";
	case ESP_ERR_NO_MEM:
		return "No memory";
	case ESP_ERR_INVALID_ARG:
		return "Invalid argument";
	case ESP_ERR_INVALID_SIZE:
		return "Invalid state";
	case ESP_ERR_INVALID_STATE:
		return "Invalid state";
	case ESP_ERR_NOT_FOUND:
		return "Not found";
	case ESP_ERR_NOT_SUPPORTED:
		return "Not supported";
	case ESP_ERR_TIMEOUT:
		return "Timeout";
	case ESP_ERR_NVS_NOT_INITIALIZED:
		return "ESP_ERR_NVS_NOT_INITIALIZED";
	case ESP_ERR_NVS_NOT_FOUND:
		return "ESP_ERR_NVS_NOT_FOUND";
	case ESP_ERR_NVS_TYPE_MISMATCH:
		return "ESP_ERR_NVS_TYPE_MISMATCH";
	case ESP_ERR_NVS_READ_ONLY:
		return "ESP_ERR_NVS_READ_ONLY";
	case ESP_ERR_NVS_NOT_ENOUGH_SPACE:
		return "ESP_ERR_NVS_NOT_ENOUGH_SPACE";
	case ESP_ERR_NVS_INVALID_NAME:
		return "ESP_ERR_NVS_INVALID_NAME";
	case ESP_ERR_NVS_INVALID_HANDLE:
		return "ESP_ERR_NVS_INVALID_HANDLE";
	case ESP_ERR_NVS_REMOVE_FAILED:
		return "ESP_ERR_NVS_REMOVE_FAILED";
	case ESP_ERR_NVS_KEY_TOO_LONG:
		return "ESP_ERR_NVS_KEY_TOO_LONG";
	case ESP_ERR_NVS_PAGE_FULL:
		return "ESP_ERR_NVS_PAGE_FULL";
	case ESP_ERR_NVS_INVALID_STATE:
		return "ESP_ERR_NVS_INVALID_STATE";
	case ESP_ERR_NVS_INVALID_LENGTH:
		return "ESP_ERR_NVS_INVALID_LENGTH";
	case ESP_ERR_WIFI_NOT_INIT:
		return "ESP_ERR_WIFI_NOT_INIT";
	//case ESP_ERR_WIFI_NOT_START:
	//	return "ESP_ERR_WIFI_NOT_START";
	case ESP_ERR_WIFI_IF:
		return "ESP_ERR_WIFI_IF";
	case ESP_ERR_WIFI_MODE:
		return "ESP_ERR_WIFI_MODE";
	case ESP_ERR_WIFI_STATE:
		return "ESP_ERR_WIFI_STATE";
	case ESP_ERR_WIFI_CONN:
		return "ESP_ERR_WIFI_CONN";
	case ESP_ERR_WIFI_NVS:
		return "ESP_ERR_WIFI_NVS";
	case ESP_ERR_WIFI_MAC:
		return "ESP_ERR_WIFI_MAC";
	case ESP_ERR_WIFI_SSID:
		return "ESP_ERR_WIFI_SSID";
	case ESP_ERR_WIFI_PASSWORD:
		return "ESP_ERR_WIFI_PASSWORD";
	case ESP_ERR_WIFI_TIMEOUT:
		return "ESP_ERR_WIFI_TIMEOUT";
	case ESP_ERR_WIFI_WAKE_FAIL:
		return "ESP_ERR_WIFI_WAKE_FAIL";
	}
	return "Unknown ESP_ERR error";
} // errorToString

bool getWifiFromBLEMes(char* ssid, char* pwd, char* dataStr) {
	uint8_t len = strlen(dataStr);
	int8_t separateIndex = -1;
	for(uint8_t i=0; i<len; i ++){
		if(dataStr[i] == WIFI_SEPARATE){
			separateIndex = i;
			break;
		} 
	}
	if (separateIndex < 0)
	{
		return false;
	}
	subString(ssid, dataStr, 0, separateIndex);
	subString(pwd, dataStr, separateIndex+1,0);
	return true;
}

/////////////Check sum////////////////////////////////////////////////////
uint8_t DecodeCheckSum(uint8_t high, uint8_t low)
{
	if (high > 0x40)
		high = high - 0x37;
	else
		high = high - 0x30;
	if (low > 0x40)
		low = low - 0x37;
	else
		low = low - 0x30;
	return ((high << 4) + low); // ket qua tra ve khac voi return (high <<4+low)
}
void EncodeCheckSum(uint8_t checksum, char *high, char *low)
{

	*high = (checksum & 0xF0) >> 4;
	*low = checksum & 0x0F;
	if ( *high < 0x0A)
		*high = *high + 0x30;
	else
		*high = *high + 0x37;
	if ( *low < 0x0A)
		*low = *low + 0x30;
	else
		*low = *low + 0x37;

}
uint8_t PROTO_CalculateChecksum(uint8_t *buf, uint32_t len) {
	uint8_t chk = 0;
	uint32_t i = 0;
	for (i = 0; i < len; i++) {
		chk ^= buf[i];
	}
	return chk;
}
//////////////end of check sum---------------------------------/////////////

uint8_t getDeviceIdFromTopic(char *deviceId, char* topic) {
	char *ch1 = strchr(topic, '/');
	char *ch2 = strchr(ch1 + 1, '/');
	// printf("ch1:%s \nch2:%s \n",ch1,ch2);
	if (ch1 == NULL || ch2 == NULL)
	{
		return 1;
	} else {
		memcpy(deviceId, ch1 + 1, ch2 - ch1 - 1);
		deviceId[ch2 - ch1 - 1] = 0;
		return 0;
	}
}

uint64_t at64i(char *dataStr) {
	uint8_t len = strlen(dataStr);
	uint64_t iData = 0;
	uint8_t index = 0;
	for (int i = len - 1; i >= 0; --i)
	{
		iData += (dataStr[i] - '0') * pow(10, index++);
	}
	return iData;
}

uint64_t valueFromHexString(char* hexStr) {
	uint8_t len = strlen(hexStr);
	uint64_t value = 0;
	for (int i = len - 1; i >= 0 ; --i)
	{
		if (hexStr[i] >= '0' && hexStr[i] <= '9')
		{
			value += (hexStr[i] - '0') * pow(16, (len - 1 - i));
		} else if (hexStr[i] >= 'A' && hexStr[i] <= 'F')
		{
			value += (hexStr[i] - 'A' + 10) * pow(16, (len - 1 - i));
		} else if (hexStr[i] >= 'a' && hexStr[i] <= 'f')
		{
			value += (hexStr[i] - 'a' + 10) * pow(16, (len - 1 - i));
		}
	}
	return value;
}

void hexStringFromValue(char* hexStr, uint64_t value) {
	uint8_t newNodeEui64[8];
	memcpy(newNodeEui64, &value, sizeof(newNodeEui64));
	sprintf(hexStr, "%02X%02X%02X%02X%02X%02X%02X%02X", newNodeEui64[7], newNodeEui64[6], newNodeEui64[5], newNodeEui64[4], newNodeEui64[3], newNodeEui64[2], newNodeEui64[1], newNodeEui64[0]);
}

/*
ex CMD: "3,3,30535,10" --> {3,3,30535,10} , index = 4
*/
#define CHARACTER_DIV ','
#define MAX_PARAM_SIZE 20
uint16_t convParamStringToNumber(uint8_t* dataString_in, uint16_t* dataNumber_out)
{
	uint16_t index = 0; //so phan tu

	char* ptr_token =NULL;
	ptr_token = strtok((char*)dataString_in, ",");
	while(ptr_token != NULL)
	{
		if(index < MAX_PARAM_SIZE)
		{
			dataNumber_out[index++] = (uint16_t)atoi((const char*)ptr_token);
			ptr_token = strtok(NULL, ",");
		}
		else break;
	}
	return index;
}

uint16_t convStringToParam(char* dataString_in, char** string_out,char* divStr)
{
	uint16_t index = 0; //so phan tu

	char* ptr_token =NULL;
	ptr_token = strtok((char*)dataString_in, divStr);
	while(ptr_token != NULL)
	{
		if(index < MAX_PARAM_SIZE)
		{
			string_out[index++] = ptr_token;
			ptr_token = strtok(NULL, divStr);
		}
		else break;
	}
	return index;
}

///////////////////////////////-------------------------MQTT
void pubTopicFromProductId(char* product_Id, char *eventType, char *localId, char *propertyCode, char* topicName){
	sprintf(topicName,"d/%s/p/%s/%s/%s",product_Id,eventType,localId,propertyCode);
}
void subTopicFromProductId(char* product_Id,char* topicName){
	sprintf(topicName,"d/%s/s/#",product_Id);
}
bool Buffer_add(uint8_t *buff,uint16_t *buffLen,uint8_t *newData,uint16_t newDataLen)
{
	memcpy(buff + *buffLen, newData,newDataLen);
	*buffLen += newDataLen;
	return true;
}