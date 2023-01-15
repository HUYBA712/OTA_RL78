#ifndef BLD_MCU_H_
#define BLD_MCU_H_

#define HAD_NEW_FIRM_MCU 1
#define NO_HAD_FIRM_MCU 0
/* **** Configuration of BootMode Entry **** */
// #define UB_PIN 17  /* UB# pin */
// #define MD_PIN 32  /* MD pin */
#define TEST_PIN 32  /* MD pin */
#define RES_PIN 33 /* RES# pin */

typedef enum
{
    BLD_STT_NONE = 0,
    BLD_STT_DOWNLOAD_FAIL,
    BLD_STT_ENTRY_FAIL,
    BLD_STT_INQUIRY_FAIL,
    BLD_STT_ID_FAIL,
    BLD_STT_ERASE_FAIL,
    BLD_STT_PROGRAM_FAIL,
    BLD_STT_VERIFY_FAIL,
    BLD_STT_SUCCESS,
    BLD_STT_START_MAX
} bld_status_t;


void bldMcu_pinInit(void);
void bldMcu_pinDeinit(void);
void bldMcu_bootModeEntry(void);
void bldMcu_bootModeRelease(void);
void bldMcu_updateInfoMcu( uint16_t hard, uint16_t firm, uint16_t model);
bool bld_binDataWriteToPartition(size_t offset_rx130, void* dst, size_t size);
void bld_createGetPartitionFileDownload(uint8_t phase);
void bldMcu_updateStatusMqtt(bld_status_t stt);
void bldMcu_taskUpdate();
void bldMcu_initPw();
void bldMcu_handleGetBslPassword(uint8_t* bslPw, uint8_t len);
#endif