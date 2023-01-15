#ifndef HTTP_HANDLER
#define HTTP_HANDLER

#include "Global.h"

#define MAX_LEN_TOCKEN 500

// callback
void addDeviceToUserDone(int result);

void Http_testCurlGet();
bool Http_getDeviceTocken();
void Http_addDeviceToUser(char *userId);
void Http_createHttpTask();
int Http_addDeviceToGateway(const char *productId);
bool Http_getScheduleList();
bool Http_sendFirmWareVer();
void testEspRequest();
bool Http_getTokenStr(char *tokenStr);

void testAddDeviceToUser();
bool Http_sendDeviceLocation(char* lat, char* lon);

bool Http_downloadFileCertificate(char *linkFile);
bool Http_downloadFileKeyMqtt(char *linkFile);

#endif