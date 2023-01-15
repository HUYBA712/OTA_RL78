/**
 ******************************************************************************
 * @file    UartTuya.c
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Nov 21, 2020
 * @brief   
 
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "UartVoice.h"
#include "HTG_Utility.h"
#include "UART_Handler.h"
#include "IroControl.h"
#include "driver/gpio.h"
#include "MqttHandler.h"
#include "BLD_mcu.h"
#include "Wifi_handler.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#ifndef DISABLE_LOG_I
#define UART_VOICE_LOG_INFO_ON
#define TAG "Uart_voice"
#endif

#ifdef UART_VOICE_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#endif

#define BUF_SIZE (1050)
#define RD_BUF_SIZE (BUF_SIZE)

#define STOP_WATER_CMD_ID 8
#define RESUME_WATER_CMD_ID 11

#define RO_200_ML_CMD_ID 9
#define VOICE_RXD_PIN (GPIO_NUM_17)
#define VOICE_TXD_PIN (GPIO_NUM_18)
/*******************************************************************************
 * External Variables
 ******************************************************************************/
extern char g_product_Id[PRODUCT_ID_LEN];
extern bool g_enTest;
uint16_t ra_mcu_firm = 0;
uint16_t ra_mcu_hard = 0;
/*******************************************************************************
 * Variables
 ******************************************************************************/

QueueHandle_t s_uartQueue;

bool s_group1IsActive = false;
bool g_mcuVoiceOtaRunning = false;


/*******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
bool UartTuya_sendMes(uint8_t command, uint16_t len, uint8_t *data);

typedef void (*cmd_ptr_function)(uint8_t *);
#define MAX_LEN_CMD_NAME (30)
#define MAX_NUM_CMD (22)
#define MAX_NUM_STR (2) //chi tach duoc thanh 2 chuoi ngan cach boi 1 ki tu ','
typedef struct
{
    const char cmd_name[MAX_LEN_CMD_NAME];
    cmd_ptr_function ptr_fun;
} command_packet_t;

void uartVoice_handleVersion(uint8_t *data_in);
void uartVoice_handleMcuToWifi(uint8_t *data_in);
void uartVoice_handleConfigWifiCmd(uint8_t *data_in);
void uartVoice_HandleRespond(uint8_t *data_in);
void uartVoice_HandleMcuReset(uint8_t *data_in);

void uartVoice_handleBleWifi(uint8_t* data_in);
void uartVoice_handleSetId(uint8_t* data_in);
void uartVoice_handleGetId(uint8_t* data_in);
void uartVoice_handleGetPassword(uint8_t* data_in);
void uartVoice_handleGetHwFw(uint8_t* data_in);
void uartVoice_handleTestReset(uint8_t* data_in);
void uartVoice_handleRegStore(uint8_t* data_in);
void uartVoice_handleRegStt(uint8_t* data_in);
void uartVoice_handleTestBootLoader(uint8_t* data_in);
void uartVoice_handleOnlineCmd(uint8_t* data_in);
void uartVoice_handleBslPassword(uint8_t* data_in);
void uartVoice_handleRequestCertificate(uint8_t* data_in);
void uartVoice_handleWaterState(uint8_t* data_in);
void uartVoice_HandleIoTestEn(uint8_t* data_in);
void uartVoice_HandleButtonTouch(uint8_t* data_in);
void uartVoice_HandleOutSet(uint8_t* data_in);
void uartVoice_HandleInCheck(uint8_t* data_in);
command_packet_t voice_cmd_user[MAX_NUM_CMD] =
    {
        {"MCU_INFO", uartVoice_handleVersion},
        {"UP", uartVoice_handleMcuToWifi},
        {"Wifi_conf", uartVoice_handleConfigWifiCmd},
        {"RSP", uartVoice_HandleRespond},
        {"MCU_RESET",uartVoice_HandleMcuReset},
        {"BLE_WIFI",uartVoice_handleBleWifi},
        {"SET_ID",uartVoice_handleSetId},
        {"GET_ID",uartVoice_handleGetId},
        {"REG_STORE",uartVoice_handleRegStore},
        {"REG_STT",uartVoice_handleRegStt},
        {"GET_PW",uartVoice_handleGetPassword},
        {"GET_HW_FW",uartVoice_handleGetHwFw},
        {"RQS_RESET",uartVoice_handleTestReset},
        {"RQS_BLD", uartVoice_handleTestBootLoader},
        {"ONLINE_CMD",uartVoice_handleOnlineCmd},
        {"BSL_PW", uartVoice_handleBslPassword},
        {"RQS_CERTIFI",uartVoice_handleRequestCertificate},
        {"WATER_STATE", uartVoice_handleWaterState},
        {"IOTEST_EN", uartVoice_HandleIoTestEn},
        {"BTN_IDX",uartVoice_HandleButtonTouch},
        {"OUT_SET",uartVoice_HandleOutSet},
        {"IN_CHECK",uartVoice_HandleInCheck},
        };

/*******************************************************************************
 * Local Function
 ******************************************************************************/
void uartVoice_HandleIoTestEn(uint8_t *data_in)
{
    UART_sendRightNow("[IOTEST_EN,0]");
}
uint8_t cntBtnIdx = 0;
void uartVoice_HandleButtonTouch(uint8_t* data_in)
{
    cntBtnIdx++;
    if(cntBtnIdx >= 9)
    {
        return;
    }
    printf("test touch: %d\n",cntBtnIdx);
    if(cntBtnIdx == 1)
    {
        bldMcu_pinInit();
    }
    if(cntBtnIdx % 2 == 0)
    {
        gpio_set_level(TEST_PIN, 1);
        gpio_set_level(RES_PIN, 0);
    }
    else{
        gpio_set_level(TEST_PIN, 0);
        gpio_set_level(RES_PIN, 1);
    }
    char buf[50] = "";
    sprintf(buf,"[BTN_IDX,%s]",data_in);
    UART_sendRightNow(buf);
}
void uartVoice_HandleOutSet(uint8_t* data_in)
{
    char buf[50] = "";
    sprintf(buf,"[OUT_SET,%s]",data_in);
    UART_sendRightNow(buf);
}
void uartVoice_HandleInCheck(uint8_t* data_in)
{
    char buf[50] = "";
    sprintf(buf,"[IN_CHECK,%s]",data_in);
    UART_sendRightNow(buf);
}
void UartVoice_updateFirmVer()
{
    char pData[50] = "";
    char pubTopicName[100] = "";
    sprintf((char*)pData,"{\"d\":%d}",ra_mcu_firm);

    pubTopicFromProductId(g_product_Id,"UP","1","mcu_voice_firmVer",pubTopicName); 
    MQTT_PublishToDeviceTopic(pubTopicName,pData);   
}
void UartVoice_logVoiceCommand(char* cmdStr)
{
    char pData[100] = "";
    char pubTopicName[100] = "";
    sprintf((char*)pData,"{\"d\":\"%s\"}",cmdStr);

    pubTopicFromProductId(g_product_Id,"UP","1","log_voiceCmd",pubTopicName); 
    MQTT_PublishToDeviceTopic(pubTopicName,pData);   
}
void UartVoice_getRaInfo()
{
    if(g_mcuVoiceOtaRunning) return;
    uint8_t byteInfo = 0xAB;
    uart_write_bytes(UART_NUM_VOICE, (char*)&byteInfo, 1);
    log_info("get info ra mcu");
}


void uartVoice_sendMeassage(char* str)
{
    if(g_mcuVoiceOtaRunning) return;
    uart_write_bytes(UART_NUM_VOICE, (char*)str, strlen(str));
    log_info("send: %s",str);
}
extern bool isWifiCofg;
extern bool g_isMqttConnected;
extern Wifi_State wifiState;
void uartVoice_sendWifiState()
{
    char buf[30];
    uint8_t wifistate = WIFI_STATE_TO_MCU_DISCONNECT;
     if(isWifiCofg)
    {
        if(wifiState == Wifi_State_Got_IP)
        {
            wifistate = WIFI_STATE_TO_MCU_CONFIG_CONNECT;
        }
        else{
           wifistate = WIFI_STATE_TO_MCU_CONFIG;
        }
    }
    else{
        if(wifiState != Wifi_State_Got_IP)
        {
            wifistate = WIFI_STATE_TO_MCU_DISCONNECT;
        }
        else{
            wifistate = WIFI_STATE_TO_MCU_CONNECT;
        }
    }
    sprintf(buf, "[WIFI_STATE,%d]",wifistate);
    uartVoice_sendMeassage(buf);
}
void uartVoice_handleVersion(uint8_t* data_in){
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
    printf("info mcu: hw %d, fw %d, md %d\n", hw, fw, md);
    ra_mcu_hard = hw; ra_mcu_firm = fw;
    UartVoice_updateFirmVer();

    if(g_enTest)
    {
        char info[60] = "";
        sprintf(info,"[MCU2_INFO,0,HW-%d,FW-%d,MD-%d]",hw, fw, md);
        UART_sendRightNow(info);
    }
    //send state wifi
    uartVoice_sendWifiState();
}


void uartVoice_sendTestEn()
{
    uartVoice_sendMeassage("[IOTEST_EN,1]");
}
void uartVoice_sendRaInfo()
{
    uartVoice_sendMeassage("[MCU_INFO,0]");
}
void uartVoice_handleMcuToWifi(uint8_t* data_in){

}
void uartVoice_handleConfigWifiCmd(uint8_t* data_in){

}
void uartVoice_HandleRespond(uint8_t* data_in){

}
void uartVoice_HandleMcuReset(uint8_t* data_in){
    //send mcu info
    UART_sendRightNow((char*)"[MCU_RESET,0]");
    uartVoice_sendMeassage("[MCU_INFO,0,]");
}
void uartVoice_handleBleWifi(uint8_t* data_in){

}
void uartVoice_handleSetId(uint8_t* data_in){

}
void uartVoice_handleGetId(uint8_t* data_in){

}
void uartVoice_handleGetPassword(uint8_t* data_in){

}
void uartVoice_handleGetHwFw(uint8_t* data_in){

}
void uartVoice_handleTestReset(uint8_t* data_in){

}
void uartVoice_handleRegStore(uint8_t* data_in){

}
void uartVoice_handleRegStt(uint8_t* data_in){

}
void uartVoice_handleTestBootLoader(uint8_t* data_in){

}
void uartVoice_handleOnlineCmd(uint8_t* data_in){

}
void uartVoice_handleBslPassword(uint8_t* data_in){

}
void uartVoice_handleRequestCertificate(uint8_t* data_in){

}
void uartVoice_handleWaterState(uint8_t* data_in){

}

void uartVoice_handleFrameMcuVoice(uint8_t *data_rcv, uint16_t length)
{
    uint8_t *buf_data_in = (uint8_t *)data_rcv;
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
            if (strcmp((const char *)ptr_arr[0], voice_cmd_user[num_cmd].cmd_name) == 0)
            {
                voice_cmd_user[num_cmd].ptr_fun(ptr_arr[1]);
                break;
            }
        }
    }
}
void UartVoice_dataHandle(char *data, uint16_t len)
{
    if (strstr(data, "group index 1 active") != NULL)
    {
        s_group1IsActive = true;
        UART_SendVoiceCmd(0);
    }
    else if (strstr(data, "group index 0 active") != NULL)
    {
        s_group1IsActive = false;
    }
    else
    {
        char *mapIdStr = strstr(data, "MapID");
        if (mapIdStr != NULL)
        {
            char commandStr[5] = "";
            char *startCmd, *endCmd;
            startCmd = strstr(mapIdStr, "=") + 1;
            // endCmd = strstr(data, "");
            if (startCmd != NULL) // && endCmd != NULL)
            {
                // memcpy(commandStr, startCmd + 1, endCmd - startCmd - 1);
                strcpy(commandStr, startCmd);
                // commandStr[endCmd - startCmd - 1] = '\0';
                int cmdIndex = atoi(commandStr);
                if (s_group1IsActive)
                {
                    // if (cmdIndex == RO_200_ML_CMD_ID)
                    // {
                    //     Iro_sendGetWaterCmd(WATER_TYPE_RO, 200, 20);
                    // }
                    // else
                    // {
                        log_info("cmd index [%d]", cmdIndex);
                        UART_SendVoiceCmd(cmdIndex);
                    // }
                }
                else if (cmdIndex == STOP_WATER_CMD_ID ||  cmdIndex == RESUME_WATER_CMD_ID)
                {
                    log_info("cmd index without trigger [%d]", cmdIndex);
                    UART_SendVoiceCmd(cmdIndex);
                }
            }
            s_group1IsActive = false;
        }
    }
    if(strstr(data,"sensor state:1") != NULL)
    {
        MQTT_PublishDataCommon("On", "LOG_SENSOR_STATE");
    }
    else if(strstr(data,"sensor state:0")!=NULL)
    {
        MQTT_PublishDataCommon("Off", "LOG_SENSOR_STATE");
    }
    //check cmd mcu ok, thi moi dung data de strtok cmdStr
    char bufCmdStr[80] = "";
    char* cmdStr_start = NULL;
    char* cmdStr_end  = NULL;
    if(strstr(data, "Get command") != NULL)
    {
        cmdStr_start = strstr(data, ": ") + 2;
        cmdStr_end = strstr(cmdStr_start, ",");
        strncpy(bufCmdStr, cmdStr_start, cmdStr_end - cmdStr_start);
        printf("VOICE_CMD_STR:%s\n",bufCmdStr);
        UartVoice_logVoiceCommand(bufCmdStr);
    }
}
static void UartVoice_rxTask(void *arg)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);
    uint16_t dataLen = 0;
    uint32_t lastTimeReceive = 0;
    while (1)
    {
        if(g_mcuVoiceOtaRunning)
        {
            vTaskDelay(100/portTICK_PERIOD_MS);
            continue;
        }
        int rxBytes = uart_read_bytes(UART_NUM_VOICE, dtmp + dataLen, 1, 1000 / portTICK_RATE_MS);
        if (rxBytes <= 0)
        {
            continue;
        }
        dataLen += rxBytes;
        printf("%c", dtmp[dataLen - 1]);
        if (dtmp[dataLen - 1] == '\r' || dtmp[dataLen - 1] == '\n' || dtmp[dataLen - 1] == ']' )
        {
            if (dataLen > 3)
            {
                dtmp[dataLen] = '\0';
                log_info("receive %d byte : %s", dataLen, dtmp);
                if(dtmp[dataLen - 1] == ']')
                {
                    uartVoice_handleFrameMcuVoice(dtmp, dataLen);
                }
                else {
                    UartVoice_dataHandle((char *)dtmp, dataLen);
                }
            }
            dataLen = 0;
        }
    }
    free(dtmp);
    vTaskDelete(NULL);
}

/*******************************************************************************
 * Public Function
 ******************************************************************************/

void UartVoice_Init()
{
    const uart_config_t uart_config = {
        .baud_rate = 115200, //115200
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(UART_NUM_VOICE, BUF_SIZE * 2, BUF_SIZE * 2, 10, &s_uartQueue, 0);
    uart_param_config(UART_NUM_VOICE, &uart_config);
    uart_set_pin(UART_NUM_VOICE, VOICE_TXD_PIN, VOICE_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(UartVoice_rxTask, "UartVoice_rxTask", 1024 * 5, NULL, 5, NULL);
}

/***********************************************/
