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
#include "OTA_http.h"
// #define READ_BIN_VIA_CACHE
#define LOG_ON
#ifdef LOG_ON
#define log_info(format,... )  ESP_LOGI( "BLD", format,  ##__VA_ARGS__) 
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

const void *map_ptr = NULL;
const esp_partition_t *partition = NULL;
bool g_mcuOtaRunning = false;
uint8_t g_newFirmMcuOta = NO_HAD_FIRM_MCU;
extern char g_product_Id[PRODUCT_ID_LEN];
uint16_t mcuFirmVer = 0;
uint16_t mcuHardVer = 0;
uint16_t mcuModelVer = 0;
bool dataIsCorrect = true;
/*can thay doi giong voi hex MSP neu lan dau tien nap ESP*/
uint8_t mcuPassWord[32] = { 
    // 0xC2,0x8E,0x02,0x8F,0x02,0x8F,0x02,0x8F,
    //                         0x74,0x8D,0x02,0x8F,0x02,0x8F,0x02,0x8F,
    //                         0x02,0x8F,0x02,0x8F,0x02,0x8F,0x02,0x8F,
    //                         0x02,0x8F,0x02,0x8F,0x02,0x8F,0xE2,0x8E};
                           0x6A,0x8F,0xAA,0x8F,0xAA,0x8F,0xAA,0x8F,
                           0xF8,0x8D,0xAA,0x8F,0xAA,0x8F,0xAA,0x8F,
                           0xAA,0x8F,0xAA,0x8F,0xAA,0x8F,0xAA,0x8F,
                           0xAA,0x8F,0xAA,0x8F,0xAA,0x8F,0x8A,0x8F};

void bldMcu_updateInfoMcu( uint16_t hard, uint16_t firm, uint16_t model)
{
    mcuFirmVer = firm;
    mcuHardVer = hard;
    mcuModelVer = model;
}

void bldMcu_pinInit(void)
{
    // /* Config for UB_PIN */
    // gpio_pad_select_gpio(UB_PIN);
    // gpio_set_direction(UB_PIN, GPIO_MODE_OUTPUT);
    // gpio_set_level(UB_PIN, 1);
    /* Config for TEST_PIN */
    gpio_pad_select_gpio(TEST_PIN);
    gpio_set_direction(TEST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(TEST_PIN, 1);
    /* Config for RES_PIN */
    gpio_pad_select_gpio(RES_PIN);
    gpio_set_direction(RES_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RES_PIN, 1);
    printf("BLD: pin init!\n");
}

void bldMcu_pinDeinit(void)
{
    gpio_set_direction(TEST_PIN, GPIO_MODE_DISABLE);
    gpio_set_direction(RES_PIN, GPIO_MODE_DISABLE);
    printf("BLD: pin deinit!\n");
}

void bldMcu_bootModeEntry(void)
{
    printf("BLD: entry boot mode\n");
    gpio_set_level(RES_PIN, 0);
    gpio_set_level(TEST_PIN, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS); 
    gpio_set_level(TEST_PIN, 1);
    vTaskDelay(1 / portTICK_PERIOD_MS); 
    gpio_set_level(TEST_PIN, 0);
    vTaskDelay(1 / portTICK_PERIOD_MS); 
    gpio_set_level(TEST_PIN, 1);
    vTaskDelay(1/ portTICK_PERIOD_MS);
    gpio_set_level(RES_PIN, 1);         /* RES# = 1 */
    vTaskDelay(1/ portTICK_PERIOD_MS);
    gpio_set_level(TEST_PIN, 0);
}

void bldMcu_bootModeRelease(void)
{
    printf("BLD: release boot mode\n");
    gpio_set_level(TEST_PIN, 0);          /* MD = high : single chip mode */
    gpio_set_level(RES_PIN, 0);         /* RES# = 0 */
    vTaskDelay(10 / portTICK_PERIOD_MS); /* Wait for 3ms over */
    gpio_set_level(RES_PIN, 1);         /* RES# = 1 */
}
extern  QueueHandle_t uart2_queue;
static void SCI_changeToParityBit(bool isEvenbit)
{
    if(isEvenbit)
        uart_set_parity(UART_NUM_2, UART_PARITY_EVEN);
    else
        uart_set_parity(UART_NUM_2, UART_PARITY_DISABLE);
}
static void SCI_changeTo(uint32_t baud, bool isNoQueue)
{
    // uart_driver_delete(UART_NUM_2);
    /* wait more than 1bit period(19200bps:52.08us) */
    vTaskDelay(52 / portTICK_PERIOD_MS); /* Wait for 52us over */
    // uart_config_t uart_config = {
    //     .baud_rate = baud,       //1000000,
    //     .data_bits = UART_DATA_8_BITS,
    //     .parity = UART_PARITY_DISABLE,
    //     .stop_bits = UART_STOP_BITS_1,
    //     .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    // uart_param_config(UART_NUM_2, &uart_config);
    // uart_set_pin(UART_NUM_2, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // if(isNoQueue)
    //     uart_driver_install(UART_NUM_2, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
    // else
    //     uart_driver_install(UART_NUM_2, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart2_queue, 0);
    uart_set_baudrate(UART_NUM_2,baud);
}
static void BootModeRelease(uint8_t mode)
{
    xQueueReset(uart2_queue);
    uart_flush_input(UART_NUM_2);
    g_mcuOtaRunning = false;
    SCI_changeTo(38400, false);
    SCI_changeToParityBit(false);
    printf("release boot mode %s\n", mode == OK ? "BLD OK": "BLD_FAIL");
    bldMcu_bootModeRelease();
    if(mode == OK)
    {
        g_newFirmMcuOta = NO_HAD_FIRM_MCU;
        FlashHandler_saveMcuOtaStt();
    }
    else{

    }
    bldMcu_pinDeinit();
}
void bldMcu_updateStatusMqtt(bld_status_t stt)
{
    char pData[50] = "";
    char pubTopicName[100] = "";
    sprintf((char*)pData,"{\"d\":%d}",stt);

    pubTopicFromProductId(g_product_Id,"UP","1","MCU_OTA_STATUS",pubTopicName); 
    MQTT_PublishToDeviceTopic(pubTopicName,pData);   
    MQTT_PublishToServerMkp(pubTopicName,pData);  
}
//offset_rx130: 0->FFFF (64k)
bool bld_binDataReadFromPartition(size_t offset_rx130, void* dst, size_t size)
{
#ifdef READ_BIN_VIA_CACHE
    memcpy((void *)(dst), (void *)(map_ptr + offset_rx130), size);
    return true;
#else
    if((offset_rx130 > 0xFFFF) || (((uint32_t)offset_rx130 + size) > 65536)) return false;
    if(esp_partition_read(partition,offset_rx130,dst,size) != ESP_OK) return false;
    return true;
#endif
}
//offset_rx130: 0->FFFF (64k)
bool bld_binDataWriteToPartition(size_t offset_rx130, void* dst, size_t size)
{
    if((offset_rx130 > 0xFFFF) || (((uint32_t)offset_rx130 + size) > 65536)) {
        printf("offset fail: %d size %d\n", offset_rx130, size);
        dataIsCorrect = false;
        return false;
    }
    if(dataIsCorrect == false) return false;

    if(esp_partition_write(partition, offset_rx130, dst, size) == ESP_OK) return true;
    else return false;
}

void bld_createGetPartitionFileDownload(uint8_t phase)
{
    // esp_err_t err;
    static bool getFirstTime = false;
    if(getFirstTime == true){
        // if(phase == PHASE_OTA_MCU_VOICE)
        // {
        //     err = esp_partition_erase_range(partition, 0, 524288);
        // }
        // else 
        // {
        //     err = esp_partition_erase_range(partition, 0, 65536);
        // }   
        // if(err == ESP_OK)
        // {
        //     printf("erase phase %d done!\n", phase);
        //     dataIsCorrect = true;
        // }
        // else{
        //     printf("erase phase %d err %d\n",phase,err);
        // }
        return;
    } 
    //su dung partition cuoi de luu file ota mcu

    // Find the partition map in the partition table
    partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");
    assert(partition != NULL);
    map_ptr = (uint32_t*)partition->address;        
    

    log_info("partition info: %s, %d, %d, %x, %d, enrypted: %d ", partition->label, partition->type, partition->subtype, partition->address, partition->size, partition->encrypted);
    printf("prepare erase partiton....\n");
    
    // if(phase == PHASE_OTA_MCU_VOICE)
    // {
    //     err = esp_partition_erase_range(partition, 0, 524288);
    // }
    // else 
    // {
    //     err = esp_partition_erase_range(partition, 0, 65536);
    // }
    // if(err == ESP_OK)
    // {
        printf("erase phase %d done!\n", phase);
        dataIsCorrect = true;
    // }
    // else{
    //     printf("erase phase %d err %d\n",phase,err);
    // }
    #ifdef READ_BIN_VIA_CACHE
    spi_flash_mmap_handle_t map_handle;
    // Map the partition to data memory
    ESP_ERROR_CHECK(esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA, &map_ptr, &map_handle));
    log_info("Mapped partition to data memory address %p ", map_ptr);

    // Read back the written verification data using the mapped memory pointer
    // char read_data[16];
    // uint32_t numLine = MAX_BYTE / 16;
    // uint32_t lastline = MAX_BYTE % 16;
    // log_info("myaddress: %p\n", map_ptr);
    // for (int index = 0; index < numLine; index++)
    // {
    //     memcpy(read_data, map_ptr + (index * 16), sizeof(read_data));
    //     for (int i = 0; i < 16; i++)
    //     {
    //         log_info("%02x ", read_data[i]);
    //     }
    //     log_info("\n");
    // }
    // for (int i = 0; i < lastline; i++)
    // {
    //     log_info("%02x ", *(uint8_t *)(map_ptr + (16 * numLine) + i));
    // }
    // log_info("\n");

    // Unmap mapped memory
    // spi_flash_munmap(map_handle);
    // log_info("Unmapped partition from data memory");
    // log_info("Example end");
    #endif
    getFirstTime = true;
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

uint16_t msp_crc16(char* pData, int length)
{
    uint8_t i;
    uint16_t wCrc = 0xffff;
    while (length--) {
        wCrc ^= *(unsigned char *)pData++ << 8;
        for (i=0; i < 8; i++)
            wCrc = wCrc & 0x8000 ? (wCrc << 1) ^ 0x1021 : wCrc << 1;
    }
    return wCrc & 0xffff;
}
void bldMcu_initPw()
{
    if(FlashHandler_getMcuPassword() == false)
    {
        FlashHandler_saveMcuPassword(); //save default pw, when program for esp and msp
    }
    log_buf_mcuRsp(mcuPassWord,32);
}
void bldMcu_sendMcuFrame(uint8_t* buf, uint16_t length)
{
    uint8_t len = uart_write_bytes(UART_NUM_2, (char *)buf, length);
    uart_wait_tx_done(UART_NUM_2, 10/portTICK_RATE_MS);
} 
bool bldMcu_receiveOk(uint8_t* bufout, uint16_t bufsize, uint16_t* lenCoreBslOut, uint16_t lenExpect)
{
    uint16_t     TimeOutCount;
    bool ret;
    TimeOutCount = 100;      
    ret = true;

    uint16_t len = 0;
    /* ---- Store the received data ---- */
    while(len < lenExpect)
    {
        uart_get_buffered_data_len(UART_NUM_2,(size_t*)&len);
        vTaskDelay(3/portTICK_RATE_MS);
        TimeOutCount--;
        if(TimeOutCount == 0)
        {
            printf("time out recsize\n");
            // break;
            return false;
        }
    }
    printf("time: %d\n",TimeOutCount); 
    len = uart_read_bytes(UART_NUM_2, bufout, bufsize, 0);
    log_buf_mcuRsp(bufout, len);
    if(len != lenExpect) return false;
    uint16_t lenCore = 0;
    if(len > 6) //0-ack, 1-0x80, 2,3-len, ... 2byte cuoi cks
    {
        if(bufout[0] != 0x00 || bufout[1] != 0x80) return false;
        lenCore = (((uint16_t)bufout[3])<<8) + bufout[2];
        if(lenCore != (len - 6)) return false;
        uint16_t checksum = msp_crc16((char*)bufout+4, lenCore);
        if((checksum>>8) != bufout[len -1]) return false;
        if((checksum & 0xFF) != bufout[len-2]) return false;
    }
    else if(len == 1)
    {
        if(bufout[0] != 0x00) return false;
    }
    else return false;
    *lenCoreBslOut = lenCore;
    return (ret);
}
bool bldMcu_commandSetBaud115200()
{
    uint8_t fr[7] = {0x80, 0x02, 0x00, 0x52, 0x06, 0x14, 0x15};
    bldMcu_sendMcuFrame((uint8_t*)fr, 7);
    uint8_t buf[1] = {0xFF};
    uint16_t lenCore;
    if(bldMcu_receiveOk((uint8_t*)buf,1,&lenCore, 1) == false) return false;
    return true;
}

bool bldMcu_commandCheckPassword()
{
    // uint8_t pw[32] = {0xCA,0x8E,0x0A,0x8F,0x0A,0x8F,0x0A,0x8F,
    //                   0x7C,0x8D,0x0A,0x8F,0x0A,0x8F,0x0A,0x8F,
    //                   0x0A,0x8F,0x0A,0x8F,0x0A,0x8F,0x0A,0x8F,
    //                   0x0A,0x8F,0x0A,0x8F,0x0A,0x8F,0xEA,0x8E};
    uint8_t fr[32+1+5]; //32byte pw,1byte 0x80,2byte len, 1byte cmd, 2byte cks, 
    fr[0] = 0x80;
    fr[1] = 0x21;
    fr[2] = 0x00;
    fr[3] = 0x11;
    memcpy(fr+4, mcuPassWord, 32);
    uint16_t cks = msp_crc16((char*)fr+3, 33);
    fr[36] = cks& 0xFF;
    fr[37] = cks>> 8;
    bldMcu_sendMcuFrame((uint8_t*)fr, 38);

    uint8_t buf[8];
    uint16_t lenCore;
    if(bldMcu_receiveOk((uint8_t*)buf,8,&lenCore, 8) == false) return false;
    if(buf[4] == 0x3B && buf[5] == 0x00) return true;
    else return false;
}
bool bldMcu_commandGetVersion()
{
    uint8_t fr[6] = {0x80, 0x01, 0x00, 0x19, 0xE8, 0x62};
    bldMcu_sendMcuFrame((uint8_t*)fr, 6);
    uint8_t buf[11];
    uint16_t lenCore;
    if(bldMcu_receiveOk((uint8_t*)buf,11,&lenCore, 11) == false) return false;
    if(buf[4] == 0x3A)
    {
        printf("4byte ver: %02X-%02X-%02X-%02X\n", buf[5],buf[6],buf[7],buf[8]);
        return true;
    }
    else return false;
}

bool bldMcu_commandSendDataBlock(uint8_t* data, uint16_t len, uint32_t addr)
{
    //addr FR2676: 8000h to 17FFFh  64K max
    if(len > 256 || len == 0) return false;
    uint8_t* fr = calloc(len+9,1);
    fr[0] = 0x80; 
    fr[1] = (len+4)& 0xFF;
    fr[2] = (len+4)>>8;
    fr[3] = 0x10;       //TX command
    fr[4] = addr&0xFF;
    fr[5] = (addr>>8) & 0x00FF;
    fr[6] = addr>> 16;
    memcpy(fr+7, data, len);
    uint16_t cks = msp_crc16((char*)fr+3, len + 4);
    fr[len+9-2] = cks& 0xFF;
    fr[len+9-1] = cks>> 8;
    bldMcu_sendMcuFrame((uint8_t*)fr, len+9);
    // log_buf_mcuRsp(fr, len+9);
    free(fr);
    uint8_t buf[8];
    uint16_t lenCore;
    if(bldMcu_receiveOk((uint8_t*)buf,8,&lenCore, 8) == false) return false;
    if(buf[4] == 0x3B && buf[5] == 0x00) return true;
    else return false;
}

#define FRAM_ADDR_START 0x8000
#define FRAM_ADDR_PW_OFFSET 0x7FE0         //+0x8000 = 0xFFE0:see device msp
uint16_t crcValueCheck[256];
bool bldMcu_commandSendData()
{
    uint32_t WriteIndex;
    uint32_t AddressIndex = 0;
    uint32_t WriteAddress = 0;
    uint8_t bufData[256];
    for(WriteIndex = 0; WriteIndex < 256; WriteIndex++)
    {
        AddressIndex = (WriteIndex << 8); /* WriteIndex * 0x100 */
        /* ---- program destination address setting ---- */
        WriteAddress = FRAM_ADDR_START + AddressIndex;
        /* ---- Sets data to be written ---- */
        if(bld_binDataReadFromPartition(AddressIndex,bufData,256) == false)
        {
            printf("read fail %X\n", AddressIndex);
            log_buf_mcuRsp(bufData,256);
        }
        if(bldMcu_commandSendDataBlock(bufData, 256, WriteAddress) == false)
        {
            return false;
        }
        crcValueCheck[WriteIndex] = msp_crc16((char*)bufData,256);
    }
    return true;
}

/*
co the read ra va memcmp su dung TX_DATA_BLOCK command hoac dung CRC check command
neu su dung CRC thi tren ESP dang doc tu partition nen k doc dc 1 luc 64k de tinh
nen so sanh 256 lan crc ngay tu luc read o command send data
*/
bool bldMcu_commandCrcCheck(uint16_t valueCrc, uint32_t addr, uint16_t length)
{
    uint8_t fr[11];
    fr[0] = 0x80;
    fr[1] = 0x06;
    fr[2] = 0x00;
    fr[3] = 0x16;
    fr[4] = addr&0xFF;
    fr[5] = (addr>>8) & 0x00FF;
    fr[6] = addr>> 16;
    fr[7] = length & 0x00FF;
    fr[8] = length>>8;
    uint16_t cks = msp_crc16((char*)fr+3, 6);
    fr[9] = cks& 0xFF;
    fr[10] = cks>> 8;
    bldMcu_sendMcuFrame((uint8_t*)fr, 11);
    uint8_t buf[9];
    uint16_t lenCore;
    if(bldMcu_receiveOk((uint8_t*)buf,9,&lenCore, 9) == false) return false;
    if(buf[4] == 0x3A){
        uint16_t cks_rsp = (((uint16_t)buf[6])<<8) + buf[5];
        printf("crc: %X - %X\n", valueCrc, cks_rsp);
        if(cks_rsp == valueCrc)
            return true;
        else 
            return false;
    } 
    else return false;
}

bool bldMcu_checkData()
{
    uint32_t WriteIndex;
    uint32_t AddressIndex = 0;
    uint32_t WriteAddress = 0;
    for(WriteIndex = 0; WriteIndex < 256; WriteIndex++)
    {
        AddressIndex = (WriteIndex << 8); /* WriteIndex * 0x100 */
        /* ---- program destination address setting ---- */
        WriteAddress = FRAM_ADDR_START + AddressIndex;
        printf("idx: %d\n",WriteIndex);
        if(bldMcu_commandCrcCheck(crcValueCheck[WriteIndex], WriteAddress, 256) == false)
        {
            return false;
        }
    }
    return true;
}


bool needGetMcuBslPw = false;
void bldMcu_handleGetBslPassword(uint8_t* bslPw, uint8_t len)
{
    memcpy(mcuPassWord, bslPw, 32);
    needGetMcuBslPw = true;
}
static void bleMcu_task2(void *prg)
{
    UART_requestBslPassword();
    uint16_t timeout = 10;
    while(timeout--)
    {
        if(needGetMcuBslPw){
            log_buf_mcuRsp(mcuPassWord,32);
            needGetMcuBslPw = false;
            break;
        } 
        vTaskDelay(200/portTICK_PERIOD_MS);
    }
    vTaskDelay(500/portTICK_PERIOD_MS);
    g_mcuOtaRunning = true;
    vTaskDelay(500/portTICK_PERIOD_MS);
    bldMcu_pinInit();
    SCI_changeTo(9600, true);
    SCI_changeToParityBit(true);
    while(1)
    {       
        bldMcu_bootModeEntry();
        uart_flush_input(UART_NUM_2);
        vTaskDelay(500/portTICK_PERIOD_MS);
        if(bldMcu_commandSetBaud115200() == true)
        {
            SCI_changeTo(115200, true);
            printf("change baud ok\n");
        }
        else {
            bldMcu_updateStatusMqtt(BLD_STT_INQUIRY_FAIL);
            printf("change baud fail\n");
            BootModeRelease(NG);
            goto ota_done1;
        }

        if(bldMcu_commandCheckPassword() == true)
        {
            printf("check pw ok\n");
        }
        else {
            bldMcu_updateStatusMqtt(BLD_STT_ID_FAIL);
            printf("check pw fail\n");
            BootModeRelease(NG);
            goto ota_done1;
        }

        if(bldMcu_commandGetVersion() == true)
        {
            printf("get version ok\n");
        }
        else {
            printf("get version fail\n");
            BootModeRelease(NG);
            goto ota_done1;
        }

        if(bldMcu_commandSendData() == true)
        {
            printf("send data ok\n");
        }
        else {
            bldMcu_updateStatusMqtt(BLD_STT_PROGRAM_FAIL);
            printf("send data fail\n");
            BootModeRelease(NG);
            goto ota_done1;
        }

        //get and save new password here
        if(bld_binDataReadFromPartition(FRAM_ADDR_PW_OFFSET,mcuPassWord,32) == true)
        {
            log_buf_mcuRsp(mcuPassWord,32);
            FlashHandler_saveMcuPassword();
        }


        if(bldMcu_checkData() == true)
        {
            printf("check data ok\n");
        }
        else {
            bldMcu_updateStatusMqtt(BLD_STT_VERIFY_FAIL);
            printf("check data fail\n");
            BootModeRelease(NG);
            goto ota_done1;
        }
       
        bldMcu_updateStatusMqtt(BLD_STT_SUCCESS);
        BootModeRelease(OK);
    ota_done1:
        log_info("Done");
        vTaskDelay(100/portTICK_PERIOD_MS);
        vTaskDelete(NULL);  
    }
}



void bldMcu_taskUpdate()
{
    if(g_mcuOtaRunning) return;
    if(dataIsCorrect == false) {
        bldMcu_updateStatusMqtt(BLD_STT_DOWNLOAD_FAIL);
        return;
    }
    xTaskCreate(bleMcu_task2, "bleMcu_task2", 4096, NULL, 5, NULL);
}





