#include "MqttHandler.h"
#include "OutputControl.h"
#include "UART_Handler.h"
#include <stdlib.h>
#include "freertos/queue.h"
#include "driver/uart.h"
#include "MKP_W_Protocol.h"
#include "Gateway_config.h"
#include "BleControl.h"
#include "freertos/queue.h"
#include "HTG_Utility.h"
#include "Wifi_handler.h"
#include "WatchDog.h"
#include "FlashHandle.h"
#include "BLD_mcu.h"
#include "HttpHandler.h"
#include "IroControl.h"
#include "BLE_handler.h"
#include "UartVoice.h"
#include "BLD_mcuVoice.h"
#ifndef DISABLE_LOG_ALL
#define UART_LOG_ON
#endif

#ifdef UART_LOG_ON
#define log_info(format, ...) ESP_LOGI("UART Handler", format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#endif
#define PATTERN_CHR_NUM (3) /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/
#define BUF_SIZE (512)
#define RD_BUF_SIZE (BUF_SIZE)

#define MAX_LEN_CMD_NAME (30)
#define MAX_NUM_CMD (22)
#define MAX_NUM_STR (2) //chi tach duoc thanh 2 chuoi ngan cach boi 1 ki tu ','
 QueueHandle_t uart2_queue;
static QueueHandle_t uartBufSendQueue = NULL;
static QueueHandle_t revOkQueue = NULL;
typedef void (*cmd_ptr_function)(uint8_t *);
//-----------------------------
int old_status = 0;
char pub_mid[15] = {0};
char pub_reason[15] = {0};
char pub_info[80] = {0};

//-----------------------------
bool g_enTest = false;
bool g_registerOk = false;
extern bool needSendFirmVer;
extern bool needSendFirmVerMqtt;
extern char g_product_Id[PRODUCT_ID_LEN];
extern char g_password[PASSWORD_LEN];
extern uint8_t g_registerStatus;
extern bool g_mcuOtaRunning;
extern uint16_t mcuFirmVer;
uint32_t timeCheckBldPin = 0;
typedef struct
{
    const char cmd_name[MAX_LEN_CMD_NAME];
    cmd_ptr_function ptr_fun;
} command_packet_t;

void UART_handleVersion(uint8_t *data_in);
void UART_handleMcuToWifi(uint8_t *data_in);
void UART_handleConfigWifiCmd(uint8_t *data_in);
void UART_HandleRespond(uint8_t *data_in);
void UART_HandleMcuReset(uint8_t *data_in);

void UART_handleBleWifi(uint8_t* data_in);
void UART_handleSetId(uint8_t* data_in);
void UART_handleGetId(uint8_t* data_in);
void UART_handleGetPassword(uint8_t* data_in);
void UART_handleGetHwFw(uint8_t* data_in);
void UART_handleTestReset(uint8_t* data_in);
void UART_handleRegStore(uint8_t* data_in);
void UART_handleRegStt(uint8_t* data_in);
void UART_handleTestBootLoader(uint8_t* data_in);
void UART_handleOnlineCmd(uint8_t* data_in);
void UART_handleBslPassword(uint8_t* data_in);
void UART_handleRequestCertificate(uint8_t* data_in);
void UART_handleWaterState(uint8_t* data_in);
void UART_HandleIoTestEn(uint8_t* data_in);
void UART_handleVersionMcuVoice(uint8_t* data_in);
void UART_HandleOutSet(uint8_t* data_in);
void UART_HandleInCheck(uint8_t* data_in);
command_packet_t cmd_user[MAX_NUM_CMD] =
    {
        {"MCU_INFO", UART_handleVersion},
        {"UP", UART_handleMcuToWifi},
        {"Wifi_conf", UART_handleConfigWifiCmd},
        {"RSP", UART_HandleRespond},
        {"MCU_RESET",UART_HandleMcuReset},
        {"ONLINE_CMD",UART_handleOnlineCmd},
        {"WATER_STATE", UART_handleWaterState},
        {"BSL_PW", UART_handleBslPassword},
        //for test jig, no need respone, retry
        {"IOTEST_EN", UART_HandleIoTestEn},
        {"MCU2_INFO", UART_handleVersionMcuVoice},      //tranh nham voi mcu pwr
        {"BLE_WIFI",UART_handleBleWifi},
        {"SET_ID",UART_handleSetId},
        {"GET_ID",UART_handleGetId},
        {"REG_STORE",UART_handleRegStore},
        {"REG_STT",UART_handleRegStt},
        {"GET_PW",UART_handleGetPassword},
        {"GET_HW_FW",UART_handleGetHwFw},
        {"RQS_RESET",UART_handleTestReset},
        {"RQS_BLD", UART_handleTestBootLoader},
        {"RQS_CERTIFI",UART_handleRequestCertificate},
        {"OUT_SET",UART_HandleOutSet},
        {"IN_CHECK",UART_HandleInCheck},
        };


void UART_HandleIoTestEn(uint8_t *data_in)
{
    g_enTest = true;
    uartVoice_sendTestEn();
}

void UART_HandleOutSet(uint8_t* data_in)
{
    char buf[50] = "";
    sprintf(buf,"[OUT_SET,%s]",data_in);
    uartVoice_sendMeassage(buf);
}
void UART_HandleInCheck(uint8_t* data_in)
{
    char buf[50] = "";
    sprintf(buf,"[IN_CHECK,%s]",data_in);
    uartVoice_sendMeassage(buf);
}
void UART_handleVersionMcuVoice(uint8_t *data_in)
{
    uartVoice_sendRaInfo();
}
void UART_handleOnlineCmd(uint8_t* data_in)
{
    uint8_t status = 1;
    xQueueOverwrite(revOkQueue, &status);
    char* pCmd = strtok((char*)data_in,",");
    if(pCmd == NULL)
    {
        return;
    }
    if(atoi(pCmd) != ONLINE_CMD_GET_STATUS)
    {
        return;
    }
    char* pstt = strtok((char*)NULL,",");
    if(pstt != NULL)
    {
        int state = atoi(pstt);
        if(state == WATER_STATE_FREE)
        {
            BLE_sendToGetWaterChar("POUR:State:Free");
        }
        else if(state == WATER_STATE_BUSY)
        {
            BLE_sendToGetWaterChar("POUR:State:Pouring");
        }
        else if(state == WATER_STATE_DONE)
        {
            BLE_sendToGetWaterChar("POUR:State:Done");
        }
    }
}
void UART_handleBslPassword(uint8_t* data_in)
{
    uint8_t status = 1;
    xQueueOverwrite(revOkQueue, &status);
    uint8_t bslPw[32];
    for(uint8_t i = 0; i < strlen((char*)data_in); i = i + 2)
    {
        bslPw[i/2] = DecodeCheckSum(data_in[i], data_in[i+1]);
    }
    bldMcu_handleGetBslPassword(bslPw, 32);
}


enum
{
    VALUE_WATER_OUT_NONE = 0,
    VALUE_WATER_OUT_RO,
    VALUE_WATER_OUT_HOT,
    VALUE_WATER_OUT_COOL,
    VALUE_WATER_OUT_COLD,
};
//"[WATER_STATE,%d,%d]",stateWaterCurrent, isVoiceCommand
void UART_handleWaterState(uint8_t* data_in)
{
    char* ptok = NULL;
    ptok = strtok((char*)data_in,",");
    if(ptok == NULL) return;
    uint8_t state = atoi(ptok);
    ptok = strtok(NULL, ",");
    if(ptok == NULL) return;
    uint8_t isVoice = atoi(ptok);
    printf("water state: %d, %d\n",state, isVoice);

    char logState[50];
    if(state == VALUE_WATER_OUT_NONE)
    {
        strcpy(logState,"Dừng");
    }
    else if(state == VALUE_WATER_OUT_RO)
    {
        strcpy(logState,"Nước tinh khiết");
    }
    else if(state == VALUE_WATER_OUT_HOT)
    {
        strcpy(logState,"Nước nóng");
    }
    else if(state == VALUE_WATER_OUT_COOL)
    {
        strcpy(logState,"Nước mát");
    }
    else if(state == VALUE_WATER_OUT_COLD)
    {
        strcpy(logState,"Nước lạnh");
    }
    if(isVoice)
    {
        strcat(logState, " (voice)");
    }
    else 
    {
        strcat(logState, " (touch)");
    }
    MQTT_PublishDataCommon(logState, "LOG_WATER_STATE");
}
//function for cmd_id1
void UART_handleVersion(uint8_t *data_in)
{
    uint8_t status = 1;
    xQueueOverwrite(revOkQueue, &status);
    char* p_hwMcu = NULL;char* p_fwMcu = NULL; char* p_modelMcu; char* ptr_tok = NULL;
    ptr_tok = strtok((char*)data_in,",");
    if(ptr_tok == NULL)
    {
        return;
    }
    p_hwMcu = ptr_tok;
    ptr_tok = strtok(NULL,",");
    if(ptr_tok == NULL)
    {
        return;
    }
    p_fwMcu = ptr_tok;
    ptr_tok = strtok(NULL,",");
    if(ptr_tok == NULL)
    {
        return;
    }
    p_modelMcu = ptr_tok;
    
    uint16_t hw = 0, fw = 0, md = 0;
    if(strncmp(p_hwMcu,"HW-",3) == 0)
    {
        hw = atoi(p_hwMcu+3);
    }
    if(strncmp(p_fwMcu,"FW-",3) == 0)
    {
        fw = atoi(p_fwMcu+3);
    }
    if(strncmp(p_modelMcu,"MD-",3) == 0)
    {
        md = atoi(p_modelMcu+3);
    }
    bldMcu_updateInfoMcu(hw,fw,md);
    printf("info mcu: hw %d, fw %d, md %d\n", hw, fw, md);
    // Http_sendFirmWareVer();
    needSendFirmVer = true;
    needSendFirmVerMqtt = true;

}
void UART_handleTestReset(uint8_t* data_in) //testwatchdog
{
    // EWD_hardRestart();
    uartVoice_sendMeassage("[RQS_RESET,1]");
}
//function for cmd_id1
bool state_bleWifi = false;
void UART_handleBleWifi(uint8_t* data_in)
{
    char buf[50] = "";
    sprintf(buf,"[BLE_WIFI,%d,]",!state_bleWifi);
    UART_sendRightNow((char*)buf);
}
// [SET_ID,<maCode>:<pw>]
void UART_handleSetId(uint8_t* data_in)
{
    g_enTest = true;
    char buf[100] = "";
    char* p_id = NULL;char* p_pw = NULL;char* ptr_tok = NULL;
    ptr_tok = strtok((char*)data_in,":");
    if(ptr_tok == NULL)
    {
        UART_sendRightNow("[SET_ID,1,]"); return;
    }
    p_id = ptr_tok;
    ptr_tok = strtok(NULL,",");
    if(ptr_tok == NULL)
    {
        UART_sendRightNow("[SET_ID,1,]"); return;
    }
    p_pw = ptr_tok;
    strcpy(g_password,p_pw);
    strcpy(g_product_Id,p_id);
    if(FlashHandler_saveDeviceInfoInStore() == true)
    {
        g_registerStatus =NOT_REGISTER;
        if(FlashHandler_saveRegisterStt() == true)  //id moi thi coi nhu dang ki lai
        {
            sprintf(buf,"[SET_ID,0,%s,%s,%s,%d,]",g_product_Id,DEVICE_TYPE_ID,g_password,DEVICE_TYPE_ID_AWS);
        }
        else{
            sprintf(buf,"[SET_ID,1,]");
        }
    }
    else{
        sprintf(buf,"[SET_ID,1,]");
    }
    UART_sendRightNow(buf);
}

void UART_handleGetId(uint8_t* data_in)  //mode test hay mode san xuat deu get ID va test wifiBle
{
    g_enTest = true;
    char buf[50] = "";
    sprintf(buf,"[GET_ID,0,%s,]",g_product_Id);
    UART_sendRightNow((char*)buf);
    GatewayConfig_init();
}

void UART_handleGetPassword(uint8_t* data_in)  
{
    char buf[50] = "";
    sprintf(buf,"[GET_PW,%s,]",g_password);
    UART_sendRightNow((char*)buf);
}
void UART_handleGetHwFw(uint8_t* data_in)
{
    char buf[50] = "";
    sprintf(buf,"[GET_HW_FW,%d,%d,]",HW_VER_NUMBER,FIRM_VER);
    UART_sendRightNow((char*)buf);
}

void UART_handleRegStore(uint8_t* data_in)     //jig request in mode san xuat
{
    g_registerStatus = HAD_REGISTER;
    if(FlashHandler_saveRegisterStt() == true)
    {
        UART_sendRightNow("[REG_STORE,0,]");
    }
    else{
        UART_sendRightNow("[REG_STORE,1,]");
    }
}
void UART_handleRegStt(uint8_t* data_in)   //jig request in mode test lai
{
    if(Http_sendFirmWareVer() == true)
    {
        g_registerStatus = HAD_REGISTER;
        if(FlashHandler_saveRegisterStt() == true)
        {
            UART_sendRightNow("[REG_STT,0,]");
        }
        else 
        {
            UART_sendRightNow("[REG_STT,1,]");
        }
    }
    else{
        UART_sendRightNow("[REG_STT,1,]");
    }
}
extern bool g_mqttHaveNewCertificate;
void UART_handleRequestCertificate(uint8_t* data_in)
{
    if(g_mqttHaveNewCertificate)
    {
        UART_sendRightNow("[RQS_CERTIFI,0,]");   //ok
        return;
    }
    //close ble to increase heap size
    // BLE_releaseBle();
    if(MQTT_PublishRequestGetCeritificate(g_product_Id) == -1)
    {
        UART_sendRightNow("[RQS_CERTIFI,1,]");   //fail
    }
    //else wait device check then respond later
}

extern bool isWifiCofg;
extern bool g_bleConfigConnected;
bool isPowerup = true;
void UART_HandleMcuReset(uint8_t *data_in)
{
    if(isWifiCofg)
    {
        if(g_bleConfigConnected)
        {
            UART_SendLedBlinkCmd(LED_BLINK_STATE_BLINK,2000);
        }
        else{
            UART_SendLedBlinkCmd(LED_BLINK_STATE_BLINK,200);
        }
    }
    else{
        if (getWifiState() != Wifi_State_Got_IP)
        {
            UART_SendLedBlinkCmd(LED_BLINK_STATE_OFF,0);
        }
        else{
            UART_SendLedBlinkCmd(LED_BLINK_STATE_ON,0);
        }
    }
    // if(isPowerup == true) {
    //     if(mcuFirmVer == 0)
    //     {
    //         UART_sendToQueueToSend("[MCU_INFO,0,]");
    //     }
    //     isPowerup = false;
    //     return;
    // }
    if(/*!isPowerup &&*/ !g_enTest)
    {
        UART_sendToQueueToSend("[MCU_INFO,0,]");
        UART_needUpdateAllParam();
    }
   
}

void UART_handleTestBootLoader(uint8_t* data_in)
{
    if(timeCheckBldPin == 0)
    {
        UART_SendCmd("[RQS_BLD,0,]73");
        vTaskDelay(500/portTICK_PERIOD_MS); //wait for jig receive

        bldMcuVoice_pinInit();
        bldMcuVoice_bootModeEntry();
        timeCheckBldPin = esp_log_timestamp();
    }
    // vTaskDelay(2000/portTICK_PERIOD_MS); //do jig giao tiep voi esp, neu delay o day k chay tiep de nhan tu mcu
    // thay the bang gui RQS_BLD 2 lan.
    else 
    {
        if((uint32_t)(esp_log_timestamp() - timeCheckBldPin) > 8000)        //khoang thoi gian tu luc reset den khi gui ban tin reset
        {
            bldMcuVoice_bootModeRelease();
            bldMcuVoice_pinDeinit();
        }
    }
}


void UART_needUpdateAllParam()
{
    char buf_data[30] = "[GET_IO,1]";
    UART_sendToQueueToSend((char*)buf_data);  //gui sang cho IRO
}
void UART_handleMcuToWifi(uint8_t *data_in)
{
    //log_info("DATA FROM IRO : %s",(const char*)data_in);
    if ((data_in == NULL) || (data_in[0] == '\0'))
        return;
    char value_iro[100] = "";
    if (handleDataMsgFromUart(data_in, (char *)value_iro) == true)
    {
        // if (g_comMode == COM_MODE_BLE)
        // {
            BLEC_sendPropertyToMobile("1", (char *)value_iro);
        // }
        // else if (g_comMode == COM_MODE_WIFI)
        // {
            MQTT_PublishValueIro("1", (char *)value_iro, NULL, NULL, NULL);
        // }
        UART_ClearPublishInfo();
    }
}
void UART_handleConfigWifiCmd(uint8_t *data_in)
{
    log_info("config wifi cmd");
    GatewayConfig_init();
}
void UART_HandleRespond(uint8_t *data_in)
{
    if(strcmp((char*)data_in, "OK") == 0)
    {
        uint8_t status = 1;
        xQueueOverwrite(revOkQueue, &status);
    }
}

void UART_sendToQueueToSend(char* buf)
{
    bufSend_t bufSend;
    uint8_t cks = PROTO_CalculateChecksum((uint8_t*)buf, strlen(buf));
    sprintf((char*)bufSend.buf, "%s%02X", buf, cks);
    xQueueSend(uartBufSendQueue, &bufSend, 100/portTICK_PERIOD_MS);
}
void UART_sendRightNow(char* buf)
{
    char buffer[120] = "";
    uint8_t cks = PROTO_CalculateChecksum((uint8_t*)buf, strlen(buf));
    sprintf((char*)buffer, "%s%02X", buf, cks);
    UART_SendCmd(buffer);
}
void UART_Init()
{
    const uart_config_t uart_config = {
        .baud_rate = 38400, //115200
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, BUF_SIZE * 2, 10, &uart2_queue, 0);
    //uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);
    //Set uart pattern detect function.
    //uart_enable_pattern_det_intr(UART_NUM_2, '+', PATTERN_CHR_NUM, 10000, 10, 10);
    //Reset the pattern queue length to record at most 20 pattern positions.
    //uart_pattern_queue_reset(UART_NUM_2, 20);
    uartBufSendQueue = xQueueCreate(5, sizeof(bufSend_t));
    revOkQueue = xQueueCreate(1, sizeof(uint8_t));
    UART_SendCmd("[RQS_RESET,0,]6C");
    UART_sendToQueueToSend("[MCU_INFO,0,]");
}

void UART_SendCmd(char *command)
{
    if(g_mcuOtaRunning) {
        printf("not send uart, ota running\n");
        return;
    }
    const int len = strlen(command);
    //Out_setDTE(1); //enable send mode 485
    const int txBytes = uart_write_bytes(UART_NUM_2, command, len);
    //vTaskDelay(15/portTICK_RATE_MS);

    //Out_setDTE(0); //enable receive mode 485
    log_info("Wrote %d bytes: %s", txBytes, command);
}
void UART_respondOk()
{
	UART_SendCmd("[RSP,OK]7F");
}
void UART_requestInfoMcu()
{
    UART_sendToQueueToSend("[MCU_INFO,0,]");
}
void UART_requestBslPassword()
{
    UART_sendToQueueToSend("[BSL_PW,0,]");
}
void UART_SendLedBlinkCmd(LedBlinkState_t blinkState,uint16_t interval)
{
		char buf_data[30] = "";
		sprintf((char *)buf_data, "[BLINK_LED,%d,%d]", blinkState, interval);
		UART_sendToQueueToSend((char *)buf_data); //gui sang cho IRO    
}

void UART_SendVoiceCmd(uint16_t cmdIndex)
{
		char buf_data[30] = "";
		sprintf((char *)buf_data, "[VOICE_CMD,%d]", cmdIndex);
		UART_sendToQueueToSend((char *)buf_data); //gui sang cho IRO    
}


void UART_SendLedBlinkCmdWithoutQueue(LedBlinkState_t blinkState,uint16_t interval)
{
		char buf_data[30] = "";
		sprintf((char *)buf_data, "[BLINK_LED,%d,%d]", blinkState, interval);
        bufSend_t bufSend;
        uint8_t cks = PROTO_CalculateChecksum((uint8_t*)buf_data, strlen(buf_data));
        sprintf((char*)bufSend.buf, "%s%02X", buf_data, cks);
        UART_SendCmd((char*)bufSend.buf);
}
//tinh lai check sum va so sanh voi 2 byte cuoi
bool UART_checkValidFrame(uint8_t* buf, uint8_t len)
{
	uint8_t cks = PROTO_CalculateChecksum(buf, len - 2);
	uint8_t cks_rsp = DecodeCheckSum(buf[len-2], buf[len-1]);
	if (cks != cks_rsp) return false;
	else return true;
}

void UART_ProcessCmd(uint8_t *data_rcv, uint16_t length)
{
    uint8_t *buf_data_in = (uint8_t *)malloc(length);
    memcpy(buf_data_in, data_rcv, length);
    uint16_t i = 0;

    uint8_t *ptr_arr[MAX_NUM_STR] = {NULL, NULL};
    uint8_t num_ptr = 0;

    if ((buf_data_in[0] == '[') && (buf_data_in[length - 1] == ']'))
    {
        ptr_arr[num_ptr] = buf_data_in + 1; //bo qua '['
        while (i < length)
        {
            if (buf_data_in[i] == ',')
            {

                num_ptr++;
                if (num_ptr == MAX_NUM_STR)
                    break;
                buf_data_in[i] = '\0'; //','-> '\0'
                ptr_arr[num_ptr] = buf_data_in + i + 1;
            }
            i++;
        }
        buf_data_in[length - 1] = '\0';
        uint8_t num_cmd = 0;
        if (ptr_arr[1] != NULL)
            log_info("cmd: %s   value: %s", (const char *)ptr_arr[0], (const char *)ptr_arr[1]);
        for (num_cmd = 0; num_cmd < MAX_NUM_CMD; num_cmd++)
        {
            if (strcmp((const char *)ptr_arr[0], cmd_user[num_cmd].cmd_name) == 0)
            {
                if(strcmp((const char*)ptr_arr[0], "UP") == 0)
                {
                    UART_respondOk();
                    vTaskDelay(100/portTICK_PERIOD_MS);

                }
                cmd_user[num_cmd].ptr_fun(ptr_arr[1]);
                break;
            }
        }
    }
    free(buf_data_in);
}
void UART_ClearPublishInfo()
{
    pub_mid[0] = '\0';
    pub_reason[0] = '\0';
    pub_info[0] = '\0';
}
void UART_UpdatePublishInfo(char *mid, char *reason, cJSON *info)
{
    if (mid != NULL)
    {
        strcpy(pub_mid, mid);
    }
    else
    {
        pub_mid[0] = '\0';
    }
    if (info != NULL)
    {
        log_info("info khac NULL");
        cJSON *temp_info = NULL;
        temp_info = cJSON_Parse("{}");
        cJSON_AddItemToArray(temp_info, info);
        log_info("info : %s", cJSON_Print(temp_info));
        sprintf(pub_info, cJSON_Print(temp_info));
    }
    else
    {
        pub_info[0] = '\0';
    }
    strcpy(pub_reason, reason);
    log_info("reason: %s, mid: %s", pub_reason, pub_mid);
}
static void rx_task(void *pvParameter)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);
    for (;;)
    {
        //  if(g_mcuOtaRunning) {
        //     vTaskDelay(10/portTICK_PERIOD_MS);
        //     continue;
        //  }
        //Waiting for UART event.
        if (xQueueReceive(uart2_queue, (void *)&event, (portTickType)portMAX_DELAY))
        { //portMAX_DELAY: block den khi nhan dc thi thoi
            if(g_mcuOtaRunning) continue;
            bzero(dtmp, RD_BUF_SIZE);
            //ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch (event.type)
            {
            //Event of UART receving data
            case UART_DATA:
                uart_read_bytes(UART_NUM_2, dtmp, event.size, portMAX_DELAY);
                dtmp[event.size] = '\0';
                if(event.size < 4) break;
                printf("Receive %d byte: %s\n", event.size, (const char *)dtmp);
                //uart_write_bytes(UART_NUM_2, (const char*) dtmp, event.size);
                if(UART_checkValidFrame(dtmp, event.size) == true)
                {
                    UART_ProcessCmd(dtmp, event.size - 2);
                }
                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                uart_flush_input(UART_NUM_2);
                xQueueReset(uart2_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                uart_flush_input(UART_NUM_2);
                xQueueReset(uart2_queue);
                break;
            //Others
            default:
                //ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

static void tx_task(void *arg)
{
    vTaskDelay(10000/portTICK_PERIOD_MS);   //wait IRO run to main
    while(1)
    {
        bufSend_t bufRev;
        if(xQueueReceive(uartBufSendQueue, &bufRev, portMAX_DELAY) == pdTRUE)
        {
            uint8_t statusRev = 0;
            for(uint8_t i = 0; i < 3; i++)
            {
                if(g_enTest)
                {
                    continue;
                }
                xQueueReset(revOkQueue);
                UART_SendCmd((char*)bufRev.buf);
                if(xQueueReceive(revOkQueue, &statusRev, 1000/portTICK_PERIOD_MS) == pdTRUE)
                {
                    if(statusRev == 1)
                    {
                        printf("receive rsp OK\n");
                        break;
                    }
                }
                else{
                    printf("retry %d\n", i);
                }   
            }
        }
    }
}

void UART_CreateReceiveTask()
{
    xTaskCreate(rx_task, "uart_rx_task", 4 * 1024, NULL, 5, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 3 * 1024, NULL, 5, NULL);
}