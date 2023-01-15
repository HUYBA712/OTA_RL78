#ifndef DATE_TIME_H
#define DATE_TIME_H
#include "Global.h"

void initDateTime();
void mount_fs();
void getTimeStartStr(char* startTimeStr,uint8_t length);
void getTimeNowStr(char* nowStr,uint8_t length);

#endif