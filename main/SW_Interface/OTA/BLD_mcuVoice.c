#include "Global.h"
#include "BLD_mcu.h"
#include "driver/gpio.h"
#include "MqttHandler.h"
#include "HTG_Utility.h"
#include "esp_partition.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include "UART_Handler.h"
#include "FlashHandle.h"
#include "BLD_mcuVoice.h"
#include "UartVoice.h"
#include "OTA_http.h"
// #define READ_BIN_VIA_CACHE
#define LOG_ON
#ifdef LOG_ON
#define log_info(format,... )  ESP_LOGI( "BLD_VOICE", format,  ##__VA_ARGS__) 
#else 
#define log_info(format,... ) 
#endif

/*******************************************************************************
   Macro definitions
   *******************************************************************************/

#define OK (1)
#define NG (0)
/*******************************************************************************
Typedef definitions
*******************************************************************************/

extern const void *map_ptr;
extern const esp_partition_t *partition;
extern bool g_mcuVoiceOtaRunning;
extern uint8_t g_newFirmMcuOta; //hien tai k dung nua
extern char g_product_Id[PRODUCT_ID_LEN];
extern uint16_t ra_mcu_firm;
extern uint16_t ra_mcu_hard;

extern bool dataIsCorrect;

#define NUM_RECORDABLE_AREAS 4      //co the get  lai trong resp get signature
typedef struct 
{
    uint8_t kindOfArea;
    uint32_t startAddr;
    uint32_t endAddr;
    uint32_t accessUnit;
    uint32_t writeUnit;
} areas_info_t;
areas_info_t areasInfo[NUM_RECORDABLE_AREAS];


bool bldMcuVoice_changeBaud(uint32_t baud);


void bldMcuVoice_updateInfoMcu( uint16_t hard, uint16_t firm)
{
    ra_mcu_firm = firm;
    ra_mcu_hard = hard;
}

void bldMcuVoice_pinInit(void)
{
    gpio_pad_select_gpio(VOICE_MD_PIN);
    gpio_set_direction(VOICE_MD_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(VOICE_MD_PIN, 1);
    /* Config for RES_PIN */
    gpio_pad_select_gpio(VOICE_RES_PIN);
    gpio_set_direction(VOICE_RES_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(VOICE_RES_PIN, 1);
    printf("BLD VOICE: pin init!\n");
}

void bldMcuVoice_pinDeinit(void)
{
    gpio_set_direction(VOICE_MD_PIN, GPIO_MODE_DISABLE);
    gpio_set_direction(VOICE_RES_PIN, GPIO_MODE_DISABLE);
    printf("BLD VOICE: pin deinit!\n");
}

void bldMcuVoice_bootModeEntry(void)
{
    printf("BLD VOICE: entry boot mode\n");
    gpio_set_level(VOICE_MD_PIN, 0);
    gpio_set_level(VOICE_RES_PIN, 0);
    vTaskDelay(5 / portTICK_PERIOD_MS); 
    gpio_set_level(VOICE_RES_PIN, 1);         /* RES# = 1 */
    vTaskDelay(3/ portTICK_PERIOD_MS);
    gpio_set_level(VOICE_MD_PIN, 1);
    gpio_pad_select_gpio(VOICE_MD_PIN);
    gpio_set_direction(VOICE_MD_PIN, GPIO_MODE_INPUT);
}

void bldMcuVoice_bootModeRelease(void)
{
    printf("BLD VOICE: release boot mode\n");
    gpio_pad_select_gpio(VOICE_MD_PIN);
    gpio_set_direction(VOICE_MD_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(VOICE_MD_PIN, 1);          /* MD = high : single chip mode */
    gpio_set_level(VOICE_RES_PIN, 0);         /* RES# = 0 */
    vTaskDelay(10 / portTICK_PERIOD_MS); /* Wait for 3ms over */
    gpio_set_level(VOICE_RES_PIN, 1);         /* RES# = 1 */
}
extern  QueueHandle_t s_uartQueue;

static void SCIVoice_changeTo(uint32_t baud, bool isNoQueue)
{
    // uart_driver_delete(UART_NUM_VOICE);
    /* wait more than 1bit period(19200bps:52.08us) */
    vTaskDelay(52 / portTICK_PERIOD_MS); /* Wait for 52us over */
    // uart_config_t uart_config = {
    //     .baud_rate = baud,       //1000000,
    //     .data_bits = UART_DATA_8_BITS,
    //     .parity = UART_PARITY_DISABLE,
    //     .stop_bits = UART_STOP_BITS_1,
    //     .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    // uart_param_config(UART_NUM_VOICE, &uart_config);
    // uart_set_pin(UART_NUM_VOICE, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // if(isNoQueue)
    //     uart_driver_install(UART_NUM_VOICE, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
    // else
    //     uart_driver_install(UART_NUM_VOICE, BUF_SIZE * 2, BUF_SIZE * 2, 20, &s_uartQueue, 0);
    uart_set_baudrate(UART_NUM_VOICE,baud);
}
static void BootModeRelease(uint8_t mode)
{
    xQueueReset(s_uartQueue);
    uart_flush_input(UART_NUM_VOICE);
    g_mcuVoiceOtaRunning = false;
    // SCIVoice_changeTo(115200, false);
    printf("release boot mode %s\n", mode == OK ? "BLD OK": "BLD_FAIL");
    bldMcuVoice_bootModeRelease();
    if(mode == OK)
    {
        g_newFirmMcuOta = NO_HAD_FIRM_MCU;
        FlashHandler_saveMcuOtaStt();
    }
    else{

    }
    bldMcuVoice_pinDeinit();
}
void bldMcuVoice_updateStatusMqtt(bld_voice_status_t stt)
{
    char pData[50] = "";
    char pubTopicName[100] = "";
    sprintf((char*)pData,"{\"d\":%d}",stt);

    pubTopicFromProductId(g_product_Id,"UP","1","MCU_VOICE_OTA_STATUS",pubTopicName); 
    MQTT_PublishToDeviceTopic(pubTopicName,pData);   
    MQTT_PublishToServerMkp(pubTopicName,pData); 
}
//offset_rx130: 0->7FFFF (512k)
bool bldVoice_binDataReadFromPartition(size_t offset_ra, void* dst, size_t size)
{
#ifdef READ_BIN_VIA_CACHE
    memcpy((void *)(dst), (void *)(map_ptr + offset_rx130), size);
    return true;
#else
    if((offset_ra > 0x20000) || (((uint32_t)offset_ra + size) > 131072)) return false;
    if(esp_partition_read(partition,offset_ra,dst,size) != ESP_OK) return false;
    return true;
#endif
}
//offset_ra6m1: 0->7FFFF (512k)
bool bldVoice_binDataWriteToPartition(size_t offset_ra, void* dst, size_t size)
{
    if((offset_ra > 0x7FFFF) || (((uint32_t)offset_ra + size) > 524288)) {
        printf("offset fail: %d size %d\n", offset_ra, size);
        dataIsCorrect = false;
        return false;
    }
    if(dataIsCorrect == false) return false;

    if(esp_partition_write(partition, offset_ra, dst, size) == ESP_OK) return true;
    else return false;
}

static void log_buf_mcuRsp(uint8_t* buf, uint16_t len)
{
    log_info("respond mcu: ");
    for (int i = 0; i < len; i++)
    {
        printf("%02X ", buf[i]);
    }
    log_info("\n");
}

uint8_t bldMcuVoice_checksum(uint8_t* buf, uint16_t length)
{
    uint8_t cks = 0;
    for(uint16_t i = 0; i < length; i++)
    {
        cks += buf[i];
    }
    cks = (~cks) + 1;
    return (uint8_t)cks;
}
void bldMcuVoice_sendMcuFrame(uint8_t* buf, uint16_t length)
{
    uint8_t len = uart_write_bytes(UART_NUM_VOICE, (char *)buf, length);
    uart_wait_tx_done(UART_NUM_VOICE, 10/portTICK_RATE_MS);
} 
bool bldMcuVoice_receiveOk(uint8_t* bufout, uint16_t bufsize, uint16_t* lenCoreBslOut, uint16_t lenExpect)
{
    uint16_t     TimeOutCount;
    bool ret;
    TimeOutCount = 1000;      
    ret = true;

    uint16_t len = 0;
    /* ---- Store the received data ---- */
    while(len < lenExpect)
    {
        uart_get_buffered_data_len(UART_NUM_VOICE,(size_t*)&len);
        vTaskDelay(3/portTICK_RATE_MS);
        TimeOutCount--;
        if(TimeOutCount == 0)
        {
            len = uart_read_bytes(UART_NUM_VOICE, bufout, bufsize, 0);
            log_buf_mcuRsp(bufout, len);
            printf("time out recsize: len %d, time %d\n", len,TimeOutCount);
            // break;
            return false;
        }
    }
    // printf("time: %d\n",TimeOutCount); 
    len = uart_read_bytes(UART_NUM_VOICE, bufout, bufsize, 0);
    log_buf_mcuRsp(bufout, len);
    if(len != lenExpect) return false;
    //0-sod, 1-lenH, 2- lenL, 3-RSPCOM, 4..., SUM, ETX
    if(bufout[0] != 0x02 || bufout[len-1] != 0x03)return false;
    uint8_t cks = bldMcuVoice_checksum(bufout+1, len - 3);
    // cks = cks + bufout[len - 2];
    if(cks != bufout[len-2])return false;
    // *lenCoreBslOut = ((uint16_t)bufout[1] << 8) + bufout[2];
    return (ret);
}

bool bldMcuVoice_synchonous()
{
    uint8_t byteSyn = 0x00;
    bldMcuVoice_sendMcuFrame(&byteSyn, 1);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    // uint8_t fr2[7] = {0x01,0x03,0x9a,0x00,0x21,0x42,0x03};
    // bldMcuVoice_sendMcuFrame(fr2,7);
    if(bldMcuVoice_changeBaud(115200))
    {
        return true;
    }
    // uint16_t timeout = 5;
    // while(timeout--)
    // {
    //     int byteRev[5] = {0xFF};;
    //     // bldMcuVoice_sendMcuFrame(&byteSyn, 1);
    //     int rxBytes = uart_read_bytes(UART_NUM_VOICE, &byteRev, 5, 1 / portTICK_RATE_MS);
    //     if(rxBytes < 0) continue;
    //     if(rxBytes == 1)
    //     {
    //         if((byteRev[0] == 0x02) && (byteRev[1] == 0x03) && (byteRev[2] == 0x06))
    //         {
    //             return true;
    //         }
    //     //     if(byteRev == 0x00)
    //     //     {
    //     //         byteSyn = 0x55;
    //     //         bldMcuVoice_sendMcuFrame(&byteSyn, 1);
    //     //         rxBytes = uart_read_bytes(UART_NUM_VOICE, &byteRev, 5, 100 / portTICK_RATE_MS);
    //     //         if(rxBytes < 0) return false;
    //     //         if(byteRev == 0xC3)
    //     //         {
    //     //             return true;
    //     //         }
    //     //         else return false;
    //     //     }
    //     }
    // }
    return false;
}
bool bldMcuVoice_resetCmd()
{
    uint8_t fr[5] = {0x01, 0x01, 0x00, 0xFF, 0x03};
    bldMcuVoice_sendMcuFrame((uint8_t*)fr, 5);
    uint8_t buf[5] = {0xFF};
    uint16_t lenCore;
    if(bldMcuVoice_receiveOk((uint8_t*)buf,5,&lenCore, 5) == false) return false;
    if(buf[2] == 0x06) return true;
    else return false;
}

bool bldMcuVoice_siliconSignatureCmd()
{
    uint8_t fr[5] = {0x01, 0x01, 0xC0, 0x3F, 0x03};
    bldMcuVoice_sendMcuFrame((uint8_t*)fr, 5);
    uint8_t buf[5] = {0xFF};
    uint8_t buff1[26] = {0xFF};
    uint16_t lenCore;
    uint16_t lenCore1;
    if(bldMcuVoice_receiveOk((uint8_t*)buf,5,&lenCore, 5) == false) return false;
    if(bldMcuVoice_receiveOk((uint8_t*)buff1,26,&lenCore1, 26) == false) return false;
    if(buf[2] == 0x06) return true;

    else return false;
}

bool bldMcuVoice_changeBaud(uint32_t baud)
{
    uint8_t fr[7] = {0x01,0x03,0x9a,0x00,0x21,0x42,0x03};
    if(baud == 115200)
    {
        fr[3] = 0x00;
    }
    else if(baud == 250000)
    {
         fr[3] = 0x01;
    }
    else if(baud == 500000)
    {
         fr[3] = 0x02;
    }
    else if(baud == 1000000)
    {
         fr[3] = 0x03;
    }
    // fr[4] = baud>>24;
    // fr[5] = baud>>16;
    // fr[6] = baud>>8;
    // fr[7] = baud;
    // fr[8] = 0 - bldMcuVoice_checksum(&fr[1],7);
    bldMcuVoice_sendMcuFrame((uint8_t*)fr, 7);
    uint8_t buf[7] = {0xFF};
    uint16_t lenCore;
    if(bldMcuVoice_receiveOk((uint8_t*)buf,7,&lenCore, 7) == false) return false;
    if(buf[0] == 0x02 && buf[1] == 0x03 && buf[2] == 0x06) return true;
    else return false;
}
bool bldMcuVoice_blockEraseCmd()
{
    uint8_t fr[8] = {0x01, 0x04, 0x22, 0x00, 0x00, 0x00, 0x00, 0x03};
    uint32_t adressOffSetCodeFlash = 0;
    uint32_t adressOffSetDataFlash = 0xF1000;
    for(int i = 0;i < 64; i++)
    {
        if(i > 0)
        {
            adressOffSetCodeFlash += 2048;
        }
        fr[3] = (uint8_t)adressOffSetCodeFlash;
        fr[4] = (uint8_t)(adressOffSetCodeFlash >> 8);
        fr[5] = (uint8_t)(adressOffSetCodeFlash >> 16);
        fr[6] =  bldMcuVoice_checksum((uint8_t*)(fr+1), 5);
        bldMcuVoice_sendMcuFrame((uint8_t*)fr, 8);
        uint8_t buf[5] = {0xFF};
        uint16_t lenCore;
        if(bldMcuVoice_receiveOk((uint8_t*)buf,5,&lenCore,5) == false) return false;
        if(buf[2] != 0x06)
            return false;

        vTaskDelay(20/portTICK_PERIOD_MS);
    }
    vTaskDelay(50/portTICK_PERIOD_MS);
    for (int j = 0; j < 32 ; j++)
    {
        if (j > 0)
        {
            adressOffSetDataFlash += 256;
        }
        fr[3] = (uint8_t)adressOffSetDataFlash;
        fr[4] = (uint8_t)(adressOffSetDataFlash >> 8);
        fr[5] = (uint8_t)(adressOffSetDataFlash >> 16);
        fr[6] =  bldMcuVoice_checksum((uint8_t*)(fr+1), 5);
        bldMcuVoice_sendMcuFrame((uint8_t*)fr, 8);
        uint8_t buf[5] = {0xFF};
        uint16_t lenCore;
        if(bldMcuVoice_receiveOk((uint8_t*)buf,5,&lenCore, 5) == false) return false;
        if(buf[2] != 0x06)
            return false;

        vTaskDelay(20/portTICK_PERIOD_MS);
    }
    return true;
}

// bool  bldMcuVoice_writeDataCmd()
// {
//     uint8_t fr[11] = {0x01, 0x07, 0x40, 0x00, 0x00, 0x00, 0xFF, 0x05, 0x00, 0xB5, 0x03};
//     bldMcuVoice_sendMcuFrame((uint8_t*)fr, 11);
//     uint8_t buf[5] = {0xFF};
//     uint16_t lenCore;
//     if(bldMcuVoice_receiveOk((uint8_t*)buf,5,&lenCore, 5) == false) return false;
//     if(buf[2] == 0x06) return true;
//     else return false;
// }




bool bldMcuVoice_getAreaInfo()
{
    uint8_t fr[7] = {0x01, 0x00, 0x02, 0x3B, 0x00,0xC3,0x03};
    uint8_t buf[23] = {0xFF};
    uint16_t lenCore;
    for(uint8_t i = 0; i < NUM_RECORDABLE_AREAS; i++)
    {
        fr[4] = i; fr[5] = 0 - bldMcuVoice_checksum(&fr[1],4);
        bldMcuVoice_sendMcuFrame((uint8_t*)fr, 7);
        if(bldMcuVoice_receiveOk((uint8_t*)buf,23,&lenCore, 23) == false) return false;
        if(buf[3] != 0x3B) return false;
        areasInfo[i].kindOfArea = buf[4];

        //swap order byte
        uint8_t tem = 0;
        tem = buf[5]; buf[5] = buf[8]; buf[8] = tem; tem = buf[6]; buf[6] = buf[7]; buf[7] = tem;
        tem = buf[9]; buf[9] = buf[12]; buf[12] = tem; tem = buf[10]; buf[10] = buf[11]; buf[11] = tem;
        tem = buf[13]; buf[13] = buf[16]; buf[16] = tem; tem = buf[14]; buf[14] = buf[15]; buf[15] = tem;
        tem = buf[17]; buf[17] = buf[20]; buf[20] = tem; tem = buf[18]; buf[18] = buf[19]; buf[19] = tem;

        areasInfo[i].startAddr = *((uint32_t*)&buf[5]);
        areasInfo[i].endAddr = *((uint32_t*)&buf[9]);
        areasInfo[i].accessUnit = *((uint32_t*)&buf[13]);
        areasInfo[i].writeUnit = *((uint32_t*)&buf[17]);
    }
    for(uint8_t i = 0; i < NUM_RECORDABLE_AREAS; i++)
    {
        printf("kind: %d, start: %08X, end: %08X, access: %08X, write: %08X\n",areasInfo[i].kindOfArea,
            areasInfo[i].startAddr,areasInfo[i].endAddr,areasInfo[i].accessUnit,areasInfo[i].writeUnit);
    }
    return true;
}

bool bldMcuVoice_eraseCommand()
{
    uint8_t fr[14] = {0x01, 0x00, 0x09, 0x12, 0x00,0x00,0x00,0x00, 0x00,0x07,0xFF,0xFF, 0xE0, 0x03};
    bldMcuVoice_sendMcuFrame((uint8_t*)fr, 14);
    uint8_t buf[7] = {0xFF};
    uint16_t lenCore;
    if(bldMcuVoice_receiveOk((uint8_t*)buf,7,&lenCore, 7) == false) return false;
    if(buf[3] == 0x12 && buf[4] == 0x00) return true;
    else return false;
}
bool bldMcuVoice_writeCommand()
{
    // uint8_t fr[14] = {0x01, 0x00, 0x09, 0x13, 0x00,0x00,0x00,0x00, 0x00,0x07,0xFF,0xFF, 0xDF, 0x03};
    // bldMcuVoice_sendMcuFrame((uint8_t*)fr, 14);
    // uint8_t buf[7] = {0xFF};
    // uint16_t lenCore;
    // if(bldMcuVoice_receiveOk((uint8_t*)buf,7,&lenCore, 7) == false) return false;
    // if(buf[3] == 0x13 && buf[4] == 0x00) return true;
    // else return false;


    uint8_t fr[11] = {0x01, 0x07, 0x40, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x01, 0xBA, 0x03};
    bldMcuVoice_sendMcuFrame((uint8_t*)fr, 11);
    uint8_t buf[5] = {0xFF};
    uint16_t lenCore;
    if(bldMcuVoice_receiveOk((uint8_t*)buf,5,&lenCore, 5) == false) return false;
    if(buf[2] == 0x06) return true;
    else return false;

}
bool bldMcuVoice_commandSendDataBlock(uint8_t* data, uint16_t len, bool isLast)
{
    // if(len > 1024 || len == 0) return false;
    // uint8_t* fr = calloc(len+6,1);
    // fr[0] = 0x81; 
    // fr[1] = (len+1)>>8;
    // fr[2] = (len+1)&0xFF;
    // fr[3] = 0x13;       //TX command
    // memcpy(fr+4, data, len);
    // uint8_t cks = 0 - bldMcuVoice_checksum(&fr[1], len + 3);
    // fr[len+6-2] = cks;
    // fr[len+6-1] = 0x03;
    // bldMcuVoice_sendMcuFrame((uint8_t*)fr, len+6);
    // // log_buf_mcuRsp(fr, len+9);
    // free(fr);
    // uint8_t buf[7];
    // uint16_t lenCore;
    // if(bldMcuVoice_receiveOk((uint8_t*)buf,7,&lenCore, 7) == false) return false;
    // if(buf[3] == 0x13 && buf[4] == 0x00) return true;
    // else return false;

    if(len > 256 || len == 0) return false;
    uint8_t* fr = calloc(len+4,1);
    fr[0] = 0x02; 
    fr[1] = 0x00;
    memcpy(fr+2, data, len);
    uint8_t cks = bldMcuVoice_checksum(&fr[1], len + 1);
    fr[len+4-2] = cks;
    fr[len+4-1] = isLast ? 0x03 : 0x17;
    bldMcuVoice_sendMcuFrame((uint8_t*)fr, len+4);
    // log_buf_mcuRsp(fr, len+9);
    free(fr);
    uint8_t buf[6];
    uint16_t lenCore;
    if(bldMcuVoice_receiveOk((uint8_t*)buf,6,&lenCore, 6) == false) return false;
    if(buf[2] == 0x06 && buf[3] == 0x06) return true;
    else return false;


}

#define FLASH_ADDR_START 0x00000000
bool bldMcuVoice_commandSendData()
{
    uint32_t WriteIndex;
    uint32_t WriteAddress = 0;  //offset
    uint8_t bufData[256];       //trong info areas dang thay MAX = 128 byte
    if(bldMcuVoice_writeCommand() == false)
    {
        return false;
    }
    printf("write command ok, start program...\n");

    for (uint16_t i = 0; i < 512; i++)
    {
        printf("i=%d",i);
        if(bldVoice_binDataReadFromPartition(WriteAddress,bufData,256) == false)
        {
            printf("read fail %X\n", WriteAddress);
            log_buf_mcuRsp(bufData,256);
        }
        if(bldMcuVoice_commandSendDataBlock(bufData,256, (i==511)? true:false) == false)
        {
            return false;
        }
        WriteAddress += 256;
    }
    


    // for(uint8_t i = 0; i < NUM_RECORDABLE_AREAS; i++)
    // {
    //     if(areasInfo[i].kindOfArea == 0x00)
    //     {
    //         if(areasInfo[i].writeUnit > 1024) return false;
    //         uint32_t numOfUnit = areasInfo[i].endAddr - areasInfo[i].startAddr + 1;
    //         numOfUnit /= areasInfo[i].writeUnit;
    //         printf("num unit idx kind %d: %d\n", i,numOfUnit);
    //         for(WriteIndex = 0; WriteIndex < numOfUnit; WriteIndex++)
    //         {
    //             if(WriteAddress % 10240 == 0)
    //                 printf("%d.addr: %08X\n",WriteIndex, WriteAddress);
    //             /* ---- Sets data to be written ---- */
    //             if(bldVoice_binDataReadFromPartition(WriteAddress,bufData,areasInfo[i].writeUnit) == false)
    //             {
    //                 printf("read fail %X\n", WriteAddress);
    //                 log_buf_mcuRsp(bufData,areasInfo[i].writeUnit);
    //             }
    //             if(bldMcuVoice_commandSendDataBlock(bufData, areasInfo[i].writeUnit, (WriteAddress+ FLASH_ADDR_START)) == false)
    //             {
    //                 return false;
    //             }
    //             /* ---- program destination address setting ---- */
    //             WriteAddress += areasInfo[i].writeUnit;
    //         }
    //     }
    // }

    
    return true;
}


bool bldMcuVoice_verifyCommand()
{
    uint8_t fr[11] = {0x01, 0x07, 0x13, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x01, 0xE7, 0x03};
    bldMcuVoice_sendMcuFrame((uint8_t*)fr, 11);
    uint8_t buf[5] = {0xFF};
    uint16_t lenCore;
    if(bldMcuVoice_receiveOk((uint8_t*)buf,5,&lenCore, 5) == false) return false;
    if(buf[2] == 0x06) return true;
    else return false;

}


bool bldMcuVoice_checkData()
{
    // uint8_t fr[14] = {0x01, 0x00, 0x09, 0x15, 0x00,0x00,0x00,0x00, 0x00,0x07,0xFF,0xFF, 0xDD, 0x03};
    // uint8_t fr_ok[7] = {0x81, 0x00, 0x02, 0x15, 0x00, 0xE9, 0x03};
    // bldMcuVoice_sendMcuFrame((uint8_t*)fr, 14);
    uint32_t WriteAddress = 0;
    uint8_t bufSend[256] = {0xFF};
    if(bldMcuVoice_verifyCommand() == false)
    {
        return false;
    }
    printf("veryfi command ok, start verify...\n");

    for (uint16_t i = 0; i < 512; i++)
    {
        if(bldMcuVoice_commandSendDataBlock(bufSend,256, (i==511)? true:false) == false)
        {
            return false;
        }
        if(bldMcuVoice_commandSendDataBlock(bufSend,256, (i==511)? true:false) == false)
        {
            return false;
        }
        WriteAddress += 256;
    }

    // for(WriteIndex = 0; WriteIndex < 512; WriteIndex++)
    // {
    //     uint16_t lenCore;
    //     if(WriteAddress % 10240 == 0)
    //         printf("idx read: %d\n",WriteIndex);
    //     if(bldMcuVoice_receiveOk((uint8_t*)buf,1030,&lenCore, 1030) == false) return false;
    //     if(buf[3] != 0x15) return false;
    //     if(lenCore == 1025)
    //     {
    //         bldVoice_binDataReadFromPartition(WriteAddress,bufSend,1024);
    //         if(memcmp(&buf[4], bufSend, 1024) != 0) return false;
    //         WriteAddress += 1024;
    //     }
    //     else return false;
    //     if(WriteIndex < 511)
    //         bldMcuVoice_sendMcuFrame((uint8_t*)fr_ok, 7);
    // }
    return true;
}

static void bldMcu_task3(void *prg)
{
    vTaskDelay(500/portTICK_PERIOD_MS);
    g_mcuVoiceOtaRunning = true;
    vTaskDelay(500/portTICK_PERIOD_MS);
    bldMcuVoice_pinInit();
    printf("init PIN\n");
    // SCIVoice_changeTo(9600, true);
    while(1)
    {       
        bldMcuVoice_bootModeEntry();
        // uart_flush_input(UART_NUM_VOICE);
        printf("synchonous prepare....\n");
        vTaskDelay(5/portTICK_PERIOD_MS);
        if(bldMcuVoice_synchonous() == true)
        {
            printf("synchonous ok\n");
        }
        else
        {
            // bldMcuVoice_updateStatusMqtt(BLD_VOICE_STT_ENTRY_FAIL);
            printf("synchonous fail\n");
            BootModeRelease(NG);
            goto ota_done1;
        }
        if(bldMcuVoice_resetCmd() == true)
        {
            printf("reset cmd ok\n");
        }
        else
        {
            // bldMcuVoice_updateStatusMqtt(BLD_VOICE_STT_INQUIRY_FAIL);
            printf("reset cmd fail\n");
            BootModeRelease(NG);
            goto ota_done1;
        }
        if(bldMcuVoice_siliconSignatureCmd() == true)
        {
            // vTaskDelay(3/portTICK_PERIOD_MS);
            // SCIVoice_changeTo(115200, true);
            printf("silicon signature ok\n");
        }
        else {
            // bldMcuVoice_updateStatusMqtt(BLD_VOICE_STT_INQUIRY_FAIL);
            printf("silicon signature fail\n");
            BootModeRelease(NG);
            goto ota_done1;
        }

        if(bldMcuVoice_blockEraseCmd() == true)
        {
            printf("erase ok\n");
        }
        else {
            // bldMcuVoice_updateStatusMqtt(BLD_VOICE_STT_INQUIRY_FAIL);
            printf("erase fail\n"); 
            BootModeRelease(NG);
            goto ota_done1;
        }
        if(bldMcuVoice_commandSendData() == true)
        {
            printf("program ok\n");
        }
        else {
            // bldMcuVoice_updateStatusMqtt(BLD_VOICE_STT_PROGRAM_FAIL);
            printf("program fail\n");
            BootModeRelease(NG);
            goto ota_done1;
        }

        if(bldMcuVoice_checkData() == true)
        {
            printf("check data ok\n");
        }
        else {
            // bldMcuVoice_updateStatusMqtt(BLD_VOICE_STT_VERIFY_FAIL);
            printf("check data fail\n");
            BootModeRelease(NG);
            goto ota_done1;
        }
       
        // bldMcuVoice_updateStatusMqtt(BLD_VOICE_STT_SUCCESS);
        BootModeRelease(OK);
    ota_done1:
        log_info("Done");
        vTaskDelay(100/portTICK_PERIOD_MS);
        vTaskDelete(NULL);  
    }
}


extern uint8_t otaPhase;
void bldMcuVoice_taskUpdate()
{
    g_mcuVoiceOtaRunning = true;
    printf("vao Boot");
    bld_createGetPartitionFileDownload(PHASE_OTA_MCU_VOICE);
    // if(g_mcuVoiceOtaRunning) return;
    // if(dataIsCorrect == false) {
    //     bldMcuVoice_updateStatusMqtt(BLD_VOICE_STT_DOWNLOAD_FAIL);
    //     return;
    // }
    xTaskCreate(bldMcu_task3, "bldMcu_task3", 4096, NULL, 5, NULL);
}


/*To do:
- add test pin boot voice mcu with jig
*/




