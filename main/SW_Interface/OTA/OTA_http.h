#ifndef OTA_HTTP_H
#define OTA_HTTP_H

#include "Global.h"
enum{
    PHASE_OTA_NONE = 0,
    PHASE_OTA_ESP,
    PHASE_OTA_MCU,
    PHASE_OTA_MCU_VOICE
};
void OTA_http_checkAndDo();
void OTA_http_DoOTA(char *linkFile, uint8_t phase);
#endif
