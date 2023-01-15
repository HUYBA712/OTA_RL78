#include "Global.h"
#include "BLD_mcu_rx.h"
#include "driver/gpio.h"
#include "MqttHandler.h"
#include "HTG_Utility.h"
#include "esp_partition.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include "UART_Handler.h"
#include "FlashHandle.h"
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
#define ROMVOL_64KB (64 * 1024)
#define ROMVOL_128KB (128 * 1024)
#define ROMVOL_256KB (256 * 1024)
#define ROMVOL_512KB (512 * 1024)
#define TARGET_ROMVOL ROMVOL_64KB

#define TARGET_DATA_ADD /*((uint32_t)0xFFF40000)*/
#if (TARGET_ROMVOL == ROMVOL_64KB)
uint32_t TARGET_ID1_ADD = 0x52261252;
uint32_t TARGET_ID2_ADD = 0x18039319;
uint32_t TARGET_ID3_ADD = 0x26026E93;
uint32_t TARGET_ID4_ADD = 0xAB717561;
#define WRITING_HEAD_ADD ((uint32_t)0xFFFF0000)
// #if     (TARGET_ROMVOL == ROMVOL_128KB)
// #define TARGET_ID1_ADD     ((uint32_t)0xFFF5FFA0)
// #define TARGET_ID2_ADD     ((uint32_t)0xFFF5FFA4)
// #define TARGET_ID3_ADD     ((uint32_t)0xFFF5FFA8)
// #define TARGET_ID4_ADD     ((uint32_t)0xFFF5FFAC)
// #define WRITING_HEAD_ADD   ((uint32_t)0xFFFE0000)
// #elif   (TARGET_ROMVOL == ROMVOL_256KB)
// #define TARGET_ID1_ADD     ((uint32_t)0xFFF7FFA0)
// #define TARGET_ID2_ADD     ((uint32_t)0xFFF7FFA4)
// #define TARGET_ID3_ADD     ((uint32_t)0xFFF7FFA8)
// #define TARGET_ID4_ADD     ((uint32_t)0xFFF7FFAC)
// #define WRITING_HEAD_ADD   ((uint32_t)0xFFFC0000)
// #else  /* (TARGET_ROMVOL == ROMVOL_512KB) */
// #define TARGET_ID1_ADD     ((uint32_t)0xFFFBFFA0)
// #define TARGET_ID2_ADD     ((uint32_t)0xFFFBFFA4)
// #define TARGET_ID3_ADD     ((uint32_t)0xFFFBFFA8)
// #define TARGET_ID4_ADD     ((uint32_t)0xFFFBFFAC)
// #define WRITING_HEAD_ADD   ((uint32_t)0xFFF80000)
#endif
#define READING_HEAD_ADD WRITING_HEAD_ADD

/*MDE value xem la big (0xfffffff8) hay little (0xffffffff) andian */
#define MDES_ADD ((uint32_t)0xFFFFFF80)

#define REV(n) ((n << 24) | (((n>>16)<<24)>>16) | (((n<<16)>>24)<<16) | (n>>24)) 

#define WRITING_TIME ((uint32_t)(TARGET_ROMVOL / 256))
#define READING_TIME WRITING_TIME
#define RES_BUF_SIZE (262)

#define OK (1)
#define NG (0)
#define ERRLOOP_ON (1)
#define ERRLOOP_OFF (0)
#define INTERVAL_ON (1)
#define INTERVAL_OFF (0)

#define RES_ACK_NORMAL (0x06)
#define RES_ACK_ID (0x16)         /* ACK:ID code check request */
#define RES_ACK_BERS_EXSPC (0x46) /* ACK:Block Erase (extended specifications) */
#define RES_ACK_MERSMD (0x56)     /* ACK:Transition to erase ready */

#define ARRAY_SIZE_OF(a) (sizeof(a) / sizeof(a[0]))

#define BUF_SIZE 1024

#define BAURATE_MAX_BLD 230400 //115200
/*******************************************************************************
Typedef definitions
*******************************************************************************/
typedef struct BOOT_CMD_s
{
    uint32_t TrnSize; /* expected value of the transmit size of command */
    uint32_t RecSize; /* expected value of the receive size of response */
    uint8_t ACKRes;   /* ACK value of response */
    uint8_t *Command; /* boot command sequence data pointer */
} BOOT_CMD_t;
static uint8_t ResponseBuffer[RES_BUF_SIZE]; /* Buffer of Command Response */
static uint8_t TransferMode;
static uint8_t ReceiveMode;
static uint8_t IDProtectMode;
static uint32_t BufferIndex; /* Index of Receive Buffer */
static uint32_t DeviceCode;
static uint32_t BlockInfoData[6];

extern const void *map_ptr;
extern const esp_partition_t *partition;
extern bool g_mcuOtaRunning;
extern uint8_t g_newFirmMcuOta;
extern char g_product_Id[PRODUCT_ID_LEN];
extern uint16_t mcuFirmVer;
extern uint16_t mcuHardVer;
extern uint16_t mcuModelVer;
extern bool dataIsCorrect;
/* ==== Command definition ==== */
/* ---- Automatic bit rate adjustment ---- */
static uint8_t CMD_BitRateAdjustment_1st[] =
    {
        0x00, 0x45, 0x00, 0x00, 0x00,
        0x00, 0x45, 0x00, 0x00, 0x00,
        0x00, 0x65, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00};

static uint8_t CMD_BitRateAdjustment_2nd[] =
    {
        0x55};

/* ---- Support device inquiry ---- */
static uint8_t CMD_EnquiryDevice[] =
    {
        0x20};

/* ---- Device Selection ---- */
static uint8_t CMD_SelectDevice[] =
    {
        0x10,
        4,
        0x00, 0x00, 0x00, 0x00,
        0x00};

/* ---- Block information inquiry ---- */
static uint8_t CMD_BlockInfo[] =
    {
        0x26,
};

/* ---- Operating frequency selection ---- */
static uint8_t CMD_OperatingFreqSel_1st[] =
    {
        0x3F,
        7,
        BAURATE_MAX_BLD/100/256, BAURATE_MAX_BLD/100%256,
        0x00, 0x00,            //0x06, 0x40,
        0x02,
        0x01,
        0x01,
        0x39};
static uint8_t CMD_OperatingFreqSel_2nd[] =
    {
        RES_ACK_NORMAL};

/* ---- Program/Erase status transition ---- */
static uint8_t CMD_PEstatusTransition[] =
    {
        0x40};

/* ---- ID code check ---- */
static uint8_t CMD_IDCodeCheck[] =
    {
        0x60,
        16,
        /* --- set the data to be written to the address FFFFFFAF~FFFFFFA0
            of the microcomputer to write the data.             ---- */
        0x00, 0x00, 0x00, 0x00, /* FFFFFFA3~FFFFFFA0 Control code, ID code 1~ ID code 3*/
        0x00, 0x00, 0x00, 0x00, /* FFFFFFA7~FFFFFFA4 ID code 4~ ID code 7 */
        0x00, 0x00, 0x00, 0x00, /* FFFFFFAB~FFFFFFA8 ID code 8~ ID code 11 */
        0x00, 0x00, 0x00, 0x00, /* FFFFFFAF~FFFFFFAC ID code 12~ ID code 15 */
        0x00};

/* ---- Erase preparation ---- */
static uint8_t CMD_ErasePreparation[] =
    {
        0x48};

/* ---- Block Erase ---- */
static uint8_t CMD_BlockErase[] =
    {
        0x59,
        4,
        0x00, 0x00, 0x00, 0x00,
        0x00};

/* ---- Boot mode status inquiry ---- */
static uint8_t CMD_BootModeStatusInquiry[] =
    {
        0x4F,
};

/* ---- User / data area program preparation ---- */
static uint8_t CMD_ProgramPreparation[] =
    {
        0x43};

/* ---- Program ---- */
static uint8_t CMD_Program[] =
    {
        0x50,
        0xFF, 0xFF, 0xFF, 0x00,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
        0x33};

/* ---- Program termination ---- */
static uint8_t CMD_ProgramTermination[] =
    {
        0x50,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xB4};

/* ---- Memory read ---- */
static uint8_t CMD_MemoryRead[] =
    {
        0x52,
        9,
        0x01,
        0xFF, 0xFA, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x00,
        0xA6};

/* ==== Command structure ==== */
/* ---- Automatic bit rate adjustment ---- */
static BOOT_CMD_t BitRateAdjustment_1st =
    {
        // ARRAY_SIZE_OF(CMD_BitRateAdjustment_1st),
        1,
        1,
        0x00,
        CMD_BitRateAdjustment_1st};
static BOOT_CMD_t BitRateAdjustment_2nd =
    {
        ARRAY_SIZE_OF(CMD_BitRateAdjustment_2nd),
        1,
        0xE6,
        CMD_BitRateAdjustment_2nd};

/* ---- Support device inquiry ---- */
static BOOT_CMD_t EnquiryDevice =
    {
        ARRAY_SIZE_OF(CMD_EnquiryDevice),
        0x3E + 3,
        0x30,
        CMD_EnquiryDevice};

/* ---- Device Selection ---- */
static BOOT_CMD_t SelectDevice =
    {
        ARRAY_SIZE_OF(CMD_SelectDevice),
        1,
        RES_ACK_BERS_EXSPC,
        CMD_SelectDevice};

/* ---- Block information inquiry ---- */
static BOOT_CMD_t BlockInfo =
    {
        ARRAY_SIZE_OF(CMD_BlockInfo),
        29,
        0x36,
        CMD_BlockInfo};

/* ---- Operating frequency selection ---- */
static BOOT_CMD_t OperatingFreqSel_1st =
    {
        ARRAY_SIZE_OF(CMD_OperatingFreqSel_1st),
        1,
        RES_ACK_NORMAL,
        CMD_OperatingFreqSel_1st};
static BOOT_CMD_t OperatingFreqSel_2nd =
    {
        ARRAY_SIZE_OF(CMD_OperatingFreqSel_2nd),
        1,
        RES_ACK_NORMAL,
        CMD_OperatingFreqSel_2nd};

/* ---- Program/Erase status transition ---- */
static BOOT_CMD_t PEstatusTransition =
    {
        ARRAY_SIZE_OF(CMD_PEstatusTransition),
        1,
        RES_ACK_NORMAL,
        CMD_PEstatusTransition};

/* ---- ID code check ---- */
static BOOT_CMD_t IDCodeCheck =
    {
        ARRAY_SIZE_OF(CMD_IDCodeCheck),
        1,
        RES_ACK_NORMAL,
        CMD_IDCodeCheck};

/* ---- Erase preparation ---- */
static BOOT_CMD_t ErasePreparation =
    {
        ARRAY_SIZE_OF(CMD_ErasePreparation),
        1,
        RES_ACK_NORMAL,
        CMD_ErasePreparation};

/* ---- Block Erase ---- */
static BOOT_CMD_t BlockErase =
    {
        ARRAY_SIZE_OF(CMD_BlockErase),
        1,
        RES_ACK_NORMAL,
        CMD_BlockErase};

/* ---- Boot mode status inquiry ---- */
static BOOT_CMD_t BootModeStatusInquiry =
    {
        ARRAY_SIZE_OF(CMD_BootModeStatusInquiry),
        5,
        0x5F,
        CMD_BootModeStatusInquiry};

/* ---- User / data area program preparation ---- */
static BOOT_CMD_t ProgramPreparation =
    {
        ARRAY_SIZE_OF(CMD_ProgramPreparation),
        1,
        RES_ACK_NORMAL,
        CMD_ProgramPreparation};

/* ---- Program ---- */
static BOOT_CMD_t Program =
    {
        ARRAY_SIZE_OF(CMD_Program),
        1,
        RES_ACK_NORMAL,
        CMD_Program};

/* ---- Program termination ---- */
static BOOT_CMD_t ProgramTermination =
    {
        ARRAY_SIZE_OF(CMD_ProgramTermination),
        1,
        RES_ACK_NORMAL,
        CMD_ProgramTermination};

/* ---- Memory read ---- */
static BOOT_CMD_t MemoryRead =
    {
        ARRAY_SIZE_OF(CMD_MemoryRead),
        262,
        0x52,
        CMD_MemoryRead};


void bldMcuRx_pinInit(void)
{
    // /* Config for UB_PIN */
    // gpio_pad_select_gpio(UB_PIN);
    // gpio_set_direction(UB_PIN, GPIO_MODE_OUTPUT);
    // gpio_set_level(UB_PIN, 1);
    /* Config for MD_PIN */
    gpio_pad_select_gpio(MD_PIN);
    gpio_set_direction(MD_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(MD_PIN, 1);
    /* Config for RES_PIN */
    gpio_pad_select_gpio(RES_PIN_RX);
    gpio_set_direction(RES_PIN_RX, GPIO_MODE_OUTPUT);
    gpio_set_level(RES_PIN_RX, 1);
    printf("BLD: pin init!\n");
}

void bldMcuRx_pinDeinit(void)
{
    gpio_set_direction(MD_PIN, GPIO_MODE_DISABLE);
    gpio_set_direction(RES_PIN_RX, GPIO_MODE_DISABLE);
    printf("BLD: pin deinit!\n");
}

void bldMcuRx_bootModeEntry(void)
{
    printf("BLD: entry boot mode\n");
    gpio_set_level(RES_PIN_RX, 0);
    gpio_set_level(MD_PIN, 0);
    // gpio_set_level(UB_PIN, 1);
    // vTaskDelay(3000 / portTICK_PERIOD_MS);
    vTaskDelay(10 / portTICK_PERIOD_MS); /* Wait for 3ms over */
    gpio_set_level(RES_PIN_RX, 1);         /* RES# = 1 */
}

void bldMcuRx_bootModeRelease(void)
{
    printf("BLD: release boot mode\n");
    gpio_set_level(MD_PIN, 1);          /* MD = high : single chip mode */
    gpio_set_level(RES_PIN_RX, 0);         /* RES# = 0 */
    vTaskDelay(10 / portTICK_PERIOD_MS); /* Wait for 3ms over */
    gpio_set_level(RES_PIN_RX, 1);         /* RES# = 1 */
}
extern  QueueHandle_t uart2_queue;
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
    printf("release boot mode %s\n", mode == OK ? "BLD OK": "BLD_FAIL");
    bldMcuRx_bootModeRelease();
    if(mode == OK)
    {
        g_newFirmMcuOta = NO_HAD_FIRM_MCU;
        FlashHandler_saveMcuOtaStt();
    }
    else{

    }
    bldMcuRx_pinDeinit();
}
void bldMcuRx_updateStatusMqtt(bld_status_t stt)
{
    char pData[50] = "";
    char pubTopicName[100] = "";
    sprintf((char*)pData,"{\"d\":%d}",stt);

    pubTopicFromProductId(g_product_Id,"UP","1","MCU_OTA_STATUS",pubTopicName); 
    MQTT_PublishToDeviceTopic(pubTopicName,pData);   
    MQTT_PublishToServerMkp(pubTopicName,pData);  
}
//offset_rx130: 0->FFFF (64k)
bool bldRx_binDataReadFromPartition(size_t offset_rx130, void* dst, size_t size)
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
bool bldRx_binDataWriteToPartition(size_t offset_rx130, void* dst, size_t size)
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
uint32_t mdeValue_get()
{
    uint32_t mdeValue = 0;
    bldRx_binDataReadFromPartition(0xFF80, &mdeValue, 4);
    // log_info("mde: %x", mdeValue);
    return mdeValue;
}


static uint8_t CalcSumData(uint8_t *pData, uint32_t Length)
{
    uint32_t loop;
    uint8_t CheckSum;

    for (loop = 0, CheckSum = 0; loop < Length; loop++)
    {
        CheckSum += *pData;
        pData++;
    }
    CheckSum = (uint8_t)(0 - CheckSum);

    return (CheckSum);
}
/*******************************************************************************
* ID            : -
* Outline       : Copies unsigned long(4 characters) data
* Header        : none
* Function Name : U4memcpy
* Description   : the copied to the storage area  corresponding to the
*               :  size of the copy destination specified.
* Arguments     : void       *pS1
*               : const void *pS2
* Return Value  : *pS1
*******************************************************************************/
static void *U4memcpy(void *pS1, const void *pS2)
{
    memcpy(pS1, pS2, 4ul);
    if (mdeValue_get() == 0xFFFFFFFF)      //dang default la little andian (copy byte thap cua pS2 thanh byte cao cua pS1)
    {
        *(uint32_t*)pS1 = REV(*(uint32_t*)pS1);
    }

    return (pS1);
}
static void TransferCommand(BOOT_CMD_t *pCmd)
{
    uint32_t TransferCount;
    log_info("cmd header: %02X\n",*pCmd->Command);
    // if(pCmd == &OperatingFreqSel_1st)
    // {
    //     printf("%x: %x \n",OperatingFreqSel_1st.Command[2],OperatingFreqSel_1st.Command[3]);
    // }
    // for (TransferCount = 0; TransferCount < pCmd->TrnSize; TransferCount++)
    // {
        uint8_t len = uart_write_bytes(UART_NUM_2, (char *)pCmd->Command, pCmd->TrnSize);
        uart_wait_tx_done(UART_NUM_2, 10/portTICK_RATE_MS);
        // if (INTERVAL_ON == TransferMode)
        // {
        //     /* ---- Wait for 1ms over ---- */
        //     vTaskDelay(1 / portTICK_PERIOD_MS);
        // }
    // }
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
static uint8_t ReceiveResponse(BOOT_CMD_t *pCmd)
{
    // uint32_t    ReceiveCount;
    uint8_t     TimeOutCount;
    uint8_t ret;
    // uint8_t     SciError;
    memset((void *)ResponseBuffer, 0xFF, sizeof(ResponseBuffer));
    if(pCmd == &BitRateAdjustment_1st)
    {
        TimeOutCount = 10;      
    }
    else TimeOutCount = 100;          /* 1s : 10ms * 100time */
    BufferIndex = 0;
    ret = OK;

    uint16_t len = 0;
    if (OK == ret)
    {
        /* ---- Store the received data ---- */
        while(len < pCmd->RecSize)
        {
            uart_get_buffered_data_len(UART_NUM_2,&len);
            vTaskDelay(10/portTICK_RATE_MS);
            TimeOutCount--;
            if(TimeOutCount == 0)
            {
                printf("time out recsize\n");
                break;
            }
        }
        //printf("time: %d\n",TimeOutCount); //rieng lenh PEStatus mat khoang 350ms, con lai khoang <50ms
        len = uart_read_bytes(UART_NUM_2, ResponseBuffer, RES_BUF_SIZE, 0);
        // BufferIndex++;
        // if (BufferIndex >= RES_BUF_SIZE)
        // {
        //     BufferIndex = 0;
        // }
        /* ---- received data check ---- */
        // for (int i = 0; i < len; i++)
        // {
        //     log_info("receive %d %02x", i, ResponseBuffer[i]);
        // }
        if (len == 0 || ResponseBuffer[0] != pCmd->ACKRes)
        {
            ret = NG;
        }
    }
    // }
    // log_buf_mcuRsp(ResponseBuffer, len);
    // if ((ERRLOOP_ON == ReceiveMode) && (NG == ret))
    // {
    //       BootModeRelease(NG);
    // }

    return (ret);
}
static void bldMcuRx_task(void *prg)
{
    g_mcuOtaRunning = true;
    bldMcuRx_pinInit();
    SCI_changeTo(19200, true);
    while(1)
    {
        /* ==== Automatic bit rate adjustment ==== */
        {
            uint8_t cnt = 0;
            uint8_t BitAdjust;
            uint8_t loop_count = 0;
            /* ---- Initialize TransferMode , ReceiveMode ---- */
            TransferMode = INTERVAL_ON;
            ReceiveMode = ERRLOOP_OFF;

            BitAdjust = NG;
            do
            {
                /* ---- Boot Mode Entry ---- */
                // uart_write_bytes(UART_NUM_1, "a", 1);
                // vTaskDelay(500/ portTICK_PERIOD_MS);
                log_info("retry boot mode\n");
                bldMcuRx_bootModeEntry();
                uart_flush_input(UART_NUM_2);
                for(cnt = 0; cnt < 10; cnt++)
                {
                    // /* ---- 1st ---- */
                    log_info("Transfer adj bitrate cnt %d", cnt);
                    TransferCommand(&BitRateAdjustment_1st);
                    BitAdjust = ReceiveResponse(&BitRateAdjustment_1st);
                    if(BitAdjust == OK) break;
                    // log_info("Receive 1st");
                }
                if(cnt >= 9 && BitAdjust == NG)
                {
                    //go to fail report
                    printf("bit adjust 1st fail\n");
                    bldMcuRx_updateStatusMqtt(BLD_STT_ENTRY_FAIL);
                     BootModeRelease(NG);
goto ota_done;
                }
            
            } while (NG == BitAdjust);

            /* 2nd */
            TransferMode = INTERVAL_OFF;
            ReceiveMode = ERRLOOP_ON;
            TransferCommand(&BitRateAdjustment_2nd);
            log_info("Transfer 2nd");
            if(ReceiveResponse(&BitRateAdjustment_2nd) == NG)
            {
                 bldMcuRx_updateStatusMqtt(BLD_STT_ENTRY_FAIL);
                 BootModeRelease(NG);
goto ota_done;
            }
            log_info("Receive 2nd");
        }

        /* ==== Support device inquiry ==== */
        {
            uint32_t DataIndex;

            DataIndex = 4; /* Device code pointer setting of little-endian */
            TransferCommand(&EnquiryDevice);
            log_info("Transfer EnquiryDevice");
            if(ReceiveResponse(&EnquiryDevice) == NG)
            {
                 bldMcuRx_updateStatusMqtt(BLD_STT_ENTRY_FAIL);
                 BootModeRelease(NG);
goto ota_done;
            }
            
            log_info("Receive EnquiryDevice");
            if (mdeValue_get() != 0xFFFFFFFF)
            {
                DataIndex = DataIndex + ResponseBuffer[3] + 1; /* Device code pointer setting of big-endian */
            }
            /* Removal of the device code */
            U4memcpy((void *)(&DeviceCode), (void *)(&ResponseBuffer[DataIndex]));
        }
        
        /* ==== Device Selection ==== */
        {
            U4memcpy((void *)(&CMD_SelectDevice[2]), (void *)(&DeviceCode));
            /* ---- 'SUM' data set ---- */
            CMD_SelectDevice[6] = CalcSumData(&CMD_SelectDevice[0], 6ul);
            /* ---- Communication of Device Selection ---- */
            TransferCommand(&SelectDevice);
            log_info("Transfer SelectDevice");
            if(ReceiveResponse(&SelectDevice) == NG)
            {
                 bldMcuRx_updateStatusMqtt(BLD_STT_INQUIRY_FAIL);
                 BootModeRelease(NG);
goto ota_done;
            }
            
            log_info("Receive SelectDevice");
        }
        
        /* ==== Block information inquiry ==== */
        {
            uint32_t DataIndex;
            uint32_t BlockInfoIndex;

            DataIndex = 4; /* "Information block 1" data pointer setting */
            TransferCommand(&BlockInfo);
            log_info("Transfer BlockInfo");
            if(ReceiveResponse(&BlockInfo) == NG)
            {
                bldMcuRx_updateStatusMqtt(BLD_STT_INQUIRY_FAIL);
                 BootModeRelease(NG);
goto ota_done;
            }
            log_info("Receive BlockInfo");
            /* Data retrieval of "block information 1-5" */
            for (BlockInfoIndex = 0; BlockInfoIndex < 6; BlockInfoIndex++)
            {
                U4memcpy((void *)(&BlockInfoData[BlockInfoIndex]), (void *)(&ResponseBuffer[DataIndex]));
                DataIndex += 4ul; /* Data pointer update of "block information n" */
                log_info("info %d: %x\n",BlockInfoIndex,BlockInfoData[BlockInfoIndex] );
            }
        }
        
        /* ==== Operating frequency selection ==== */
        {
            /* ---- 1st ---- */
            CMD_OperatingFreqSel_1st[9] = CalcSumData(&CMD_OperatingFreqSel_1st[0], 9ul);
            TransferCommand(&OperatingFreqSel_1st);
            log_info("Transfer OperatingFreqSel_1st");
            if(ReceiveResponse(&OperatingFreqSel_1st) == NG)
            {
                bldMcuRx_updateStatusMqtt(BLD_STT_INQUIRY_FAIL);
                 BootModeRelease(NG);
goto ota_done;
            }
            
            log_info("Receive OperatingFreqSel_1st");
            /* ---- Communication speed change (1Mbps) ---- */
            SCI_changeTo(BAURATE_MAX_BLD, true);
            /* ---- 2nd ---- */
            TransferCommand(&OperatingFreqSel_2nd);
            log_info("Transfer OperatingFreqSel_2nd");
            if(ReceiveResponse(&OperatingFreqSel_2nd) == NG)
            {
                bldMcuRx_updateStatusMqtt(BLD_STT_INQUIRY_FAIL);
                 BootModeRelease(NG);
goto ota_done;
            }
            log_info("Receive OperatingFreqSel_2nd");
        }
        
        /* ==== Program/Erase status transition ==== */
        ReceiveMode = ERRLOOP_OFF;
        TransferCommand(&PEstatusTransition);
        log_info("Transfer PEstatusTransition");
        if (OK == ReceiveResponse(&PEstatusTransition))
        {
            IDProtectMode = 0;
            log_info("Receive PEstatusTransition OK");
        }
        else
        {
            if (ResponseBuffer[0] == RES_ACK_ID)
            {
                IDProtectMode = 1;
                log_info("Receive PEstatusTransition RES_ACK_ID");
            }
            else if (ResponseBuffer[0] == RES_ACK_MERSMD)
            {
                IDProtectMode = 2;
                log_info("Receive PEstatusTransition RES_ACK_MERSMD");
            }
            else
            {
                bldMcuRx_updateStatusMqtt(BLD_STT_ID_FAIL);
                 BootModeRelease(NG);
goto ota_done;
                log_info("Receive PEstatusTransition BootModeRelease");
            }
        }

        /* ==== ID code check ==== */
        if (1 == IDProtectMode)
        {
            /* ---- obtain the data to be written to the address FFFFFFAF~FFFFFFA0
                    of the microcomputer to write the data.                ---- */
            U4memcpy((void *)(&CMD_IDCodeCheck[2]), (void*)&TARGET_ID1_ADD);
            U4memcpy((void *)(&CMD_IDCodeCheck[6]), (void*)&TARGET_ID2_ADD);
            U4memcpy((void *)(&CMD_IDCodeCheck[10]), (void*)&TARGET_ID3_ADD);
            U4memcpy((void *)(&CMD_IDCodeCheck[14]), (void*)&TARGET_ID4_ADD);
            /* ---- 'SUM' data set ---- */
            CMD_IDCodeCheck[18] = CalcSumData(&CMD_IDCodeCheck[0], 18ul);
            /* ---- Communication of ID code check ---- */
            TransferCommand(&IDCodeCheck);
            log_info("Transfer IDCodeCheck");
            if (OK == ReceiveResponse(&IDCodeCheck))
            {
                IDProtectMode = 0;
                log_info("Receive IDCodeCheck OK");
            }
            else
            {
                if (ResponseBuffer[0] == RES_ACK_MERSMD)
                {
                    IDProtectMode = 2;
                    log_info("Receive IDCodeCheck RES_ACK_MERSMD");
                }
                else
                {
                    bldMcuRx_updateStatusMqtt(BLD_STT_ID_FAIL);
                     BootModeRelease(NG);
goto ota_done;
                    log_info("Receive IDCodeCheck BootModeRelease");
                }
            }
        }
        ReceiveMode = ERRLOOP_ON;
        
        /* ==== Erase preparation ==== */
        TransferCommand(&ErasePreparation);
        log_info("Transfer ErasePreparation");
        if(ReceiveResponse(&ErasePreparation) == NG)
        {
            bldMcuRx_updateStatusMqtt(BLD_STT_ERASE_FAIL);
             BootModeRelease(NG);
goto ota_done;
        }
        log_info("Receive ErasePreparation");

        /* ==== Erases all Blocks ==== */
        {
            uint32_t EraseIndex;
            uint32_t EraseIndexMax;
            uint32_t EraseAddress;
            uint32_t AddrInterval;
            uint32_t BlockInfoMax;
            uint32_t BlockInfo;

            EraseIndexMax = 1;
            if (2 == IDProtectMode)
            {
                EraseIndexMax = 2;
            }

            for (EraseIndex = 0ul; EraseIndex < EraseIndexMax; EraseIndex++)
            {
                if(EraseIndex == 0)
                {
                    EraseAddress = 0xFFFF0000; //BlockInfoData[0 + (3 * EraseIndex)];
                    AddrInterval = BlockInfoData[1 + (3 * EraseIndex)];
                    BlockInfoMax = 64;//BlockInfoData[2 + (3 * EraseIndex)];
                }
                else 
                {
                    EraseAddress = BlockInfoData[0 + (3 * EraseIndex)];
                    AddrInterval = BlockInfoData[1 + (3 * EraseIndex)];
                    BlockInfoMax = BlockInfoData[2 + (3 * EraseIndex)];
                }
                
                log_info("erase block index %d,  Start %X, size %d, numBlock %d\n",EraseIndex, EraseAddress, AddrInterval, BlockInfoMax);
                for (BlockInfo = 0ul; BlockInfo < BlockInfoMax; BlockInfo++)
                {
                    U4memcpy((void *)(&CMD_BlockErase[2]), (void *)(&EraseAddress));
                    /* ---- 'SUM' data set ---- */
                    CMD_BlockErase[6] = CalcSumData(&CMD_BlockErase[0], 6ul);
                    /* ---- Communication of Erase ---- */
                    TransferCommand(&BlockErase);
                    log_info("Transfer BlockErase num %d, add %X",BlockInfoMax-BlockInfo-1, EraseAddress);
                    
                    if(ReceiveResponse(&BlockErase) == NG)
                    {
                        bldMcuRx_updateStatusMqtt(BLD_STT_ERASE_FAIL);
                         BootModeRelease(NG);
goto ota_done;
                    }
                    log_info("Receive BlockErase");
                    EraseAddress += AddrInterval;
                }
            }
        }
        
        /* ==== Erase termination ==== */
        memset((void *)(&CMD_BlockErase[2]), 0xFF, 4ul);
        /* ---- 'SUM' data set ---- */
        CMD_BlockErase[6] = CalcSumData(&CMD_BlockErase[0], 6ul);
        /* ---- Communication of Erase ---- */
        TransferCommand(&BlockErase);
        log_info("Transfer BlockErase 2");
        if( ReceiveResponse(&BlockErase) == NG)
        {
            bldMcuRx_updateStatusMqtt(BLD_STT_ERASE_FAIL);
             BootModeRelease(NG);
goto ota_done;
        }
    
        log_info("Receive BlockErase 2");
    
        
        /* ==== Boot mode status inquiry ==== */
        TransferCommand(&BootModeStatusInquiry);
        log_info("Transfer BootModeStatusInquiry");
        if(ReceiveResponse(&BootModeStatusInquiry) == NG)
        {
            bldMcuRx_updateStatusMqtt(BLD_STT_PROGRAM_FAIL);
             BootModeRelease(NG);
goto ota_done;
        }
        
        log_info("Receive BootModeStatusInquiry");

        
        /* ==== User / data area program preparation ==== */
        TransferCommand(&ProgramPreparation);
        log_info("Transfer ProgramPreparation");
        if(ReceiveResponse(&ProgramPreparation) == NG)
        {
            bldMcuRx_updateStatusMqtt(BLD_STT_PROGRAM_FAIL);
             BootModeRelease(NG);
goto ota_done;
        }
        
        log_info("Receive ProgramPreparation");
        
        /* ==== write the data of 256 bytes(To write the target data) ==== */
        {
            uint32_t WriteIndex;
            uint32_t AddressIndex;
            uint32_t WriteAddress;
            log_info("Mapped partition to data memory address %p ", map_ptr);
            for (WriteIndex = 0; WriteIndex < WRITING_TIME; WriteIndex++)
            {
                AddressIndex = (WriteIndex << 8); /* WriteIndex * 0x100 */
                /* ---- program destination address setting ---- */
                WriteAddress = WRITING_HEAD_ADD + AddressIndex;
                U4memcpy((void *)(&CMD_Program[1]), (void *)(&WriteAddress));
                /* ---- Sets data to be written ---- */

                if(bldRx_binDataReadFromPartition(AddressIndex,&CMD_Program[5],256) == false)
                {
                    printf("read fail %X\n", AddressIndex);
                    log_buf_mcuRsp(&CMD_Program[5],256);

                }
                //memcpy((void *)(&CMD_Program[5]), (void *)(map_ptr + AddressIndex), 256ul);
                /* ---- 'SUM' data set ---- */
                CMD_Program[261] = CalcSumData(&CMD_Program[0], 261ul);
                /* ---- Communication of Program ---- */
                TransferCommand(&Program);
                log_info("Transfer Program address %X",WriteAddress);
                if(ReceiveResponse(&Program) == NG)
                {
                    bldMcuRx_updateStatusMqtt(BLD_STT_PROGRAM_FAIL);
                     BootModeRelease(NG);
goto ota_done;
                }
                
                log_info("Receive Program");
        
            }
        }
        
        /* ==== Program termination ==== */
        TransferCommand(&ProgramTermination);
        log_info("Transfer ProgramTermination");
        if(ReceiveResponse(&ProgramTermination) == NG)
        {
            bldMcuRx_updateStatusMqtt(BLD_STT_PROGRAM_FAIL);
             BootModeRelease(NG);
goto ota_done;
        }
       
        log_info("Receive ProgramTermination");

        
        /* ==== Memory read ==== */
        {
            uint32_t ReadIndex;
            uint32_t AddressIndex;
            uint32_t ReadAddress;
            char buf_readback[256];
            for (ReadIndex = 0; ReadIndex < READING_TIME; ReadIndex++)
            {
                AddressIndex = (ReadIndex << 8); /* ReadIndex * 0x100 */
                /* ---- memory-read destination address setting ---- */
                ReadAddress = READING_HEAD_ADD + AddressIndex;
                U4memcpy((void *)(&CMD_MemoryRead[3]), (void *)(&ReadAddress));
                /* ---- 'SUM' data set ---- */
                CMD_MemoryRead[11] = CalcSumData(&CMD_MemoryRead[0], 11ul);
                /* ---- Communication of memory-read ---- */
                TransferCommand(&MemoryRead);
                log_info("Transfer MemoryRead");
                if(ReceiveResponse(&MemoryRead) == NG)
                {
                    bldMcuRx_updateStatusMqtt(BLD_STT_VERIFY_FAIL);
                     BootModeRelease(NG);
goto ota_done;
                }
                
                log_info("Receive MemoryRead");
                /* ---- Verify (256byte units) ---- */
                bldRx_binDataReadFromPartition(AddressIndex,buf_readback,256);
                if (0 != strncmp((void *)&ResponseBuffer[5], (void *)buf_readback, 256ul))
                {
                    log_info("cmp fail, BootModeRelease %d",ReadIndex);
                    log_buf_mcuRsp((uint8_t*)&ResponseBuffer[0], 262);
                    uint8_t cks = CalcSumData(&CMD_MemoryRead[5], 261);
                     log_info("cks : %02X",cks);
                    log_buf_mcuRsp((uint8_t*)buf_readback, 256);
                    bldMcuRx_updateStatusMqtt(BLD_STT_VERIFY_FAIL);
                    BootModeRelease(NG);
goto ota_done;
                }
                // vTaskDelay(10/portTICK_PERIOD_MS);
            }
        }
        bldMcuRx_updateStatusMqtt(BLD_STT_SUCCESS);
        BootModeRelease(OK);
ota_done:
        log_info("Done");
        vTaskDelay(100/portTICK_PERIOD_MS);
        vTaskDelete(NULL);  
    }
}
void bldMcuRx_taskUpdate()
{
    if(g_mcuOtaRunning) return;
    if(dataIsCorrect == false) {
        bldMcuRx_updateStatusMqtt(BLD_STT_DOWNLOAD_FAIL);
        return;
    }
    xTaskCreate(bldMcuRx_task, "bldMcuRx_task", 4096, NULL, 5, NULL);
}