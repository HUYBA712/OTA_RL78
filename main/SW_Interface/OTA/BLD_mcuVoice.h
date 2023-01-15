#ifndef BLD_MCU_VOICE_H_
#define BLD_MCU_VOICE_H_


/* **** Configuration of BootMode Entry **** */
#define VOICE_RES_PIN 21  /* RESET# pin */
#define VOICE_MD_PIN 19  /* MD pin */


typedef enum
{
    BLD_VOICE_STT_NONE = 0,
    BLD_VOICE_STT_DOWNLOAD_FAIL,
    BLD_VOICE_STT_ENTRY_FAIL,
    BLD_VOICE_STT_INQUIRY_FAIL,
    BLD_VOICE_STT_ID_FAIL,
    BLD_VOICE_STT_ERASE_FAIL,
    BLD_VOICE_STT_PROGRAM_FAIL,
    BLD_VOICE_STT_VERIFY_FAIL,
    BLD_VOICE_STT_SUCCESS,
    BLD_VOICE_STT_START_MAX
} bld_voice_status_t;
void bldMcuVoice_pinInit(void);
void bldMcuVoice_pinDeinit(void);
void bldMcuVoice_bootModeEntry(void);
void bldMcuVoice_bootModeRelease(void);
void bldMcuVoice_taskUpdate();
bool bldVoice_binDataWriteToPartition(size_t offset_ra, void* dst, size_t size);
void bldMcuVoice_updateStatusMqtt(bld_voice_status_t stt);
#endif