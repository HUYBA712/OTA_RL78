/**
 ******************************************************************************
 * @file    Smarthome_HT_Gateway_W\main\BLE_handler.c
 * @author  Makipos Co.,LTD.
 * @version 1.0
 * @date    Sep 25, 2017
 * @brief
 * @history
 ******************************************************************************/
/*******************************************************************************
 * Include
 ******************************************************************************/
#include "BLE_handler.h"
#include "HTG_Utility.h"

#include "esp_bt.h"
// #include "bta_api.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "UART_Handler.h"

#include "Wifi_handler.h"
#include "gateway_config.h"
#include "HttpHandler.h"
#include "FlashHandle.h"
#include "BleControl.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define GATTS_TAG "BLE_HANDLER"

#ifndef DISABLE_LOG_I
#define BLE_HANDLER_LOG_INFO_ON
#endif

#ifdef BLE_HANDLER_LOG_INFO_ON
#define log_info(format, ...) ESP_LOGI("BLE_HANDLER", format, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#endif

#define log_error(format, ...) ESP_LOGE("BLE_HANDLER", format, ##__VA_ARGS__)

#define GATTS_SERVICE_UUID_TEST_A 0x12AB
#define GATTS_SERVICE_UUID_BLE_CONTROL 0x12AC

#define GATTS_CHAR_UUID_WIFI_LIST 0xAB01
#define GATTS_CHAR_UUID_COM 0xAB02
#define GATTS_CHAR_UUID_DEVICE_INFO 0xAB03

#define GATTS_CHAR_UUID_BLE_GET_WATER 0x109A

#define GATTS_CHAR_UUID_BLE_CONTROL_COM 0xAC02
#define GATTS_CHAR_UUID_PRO_BOM 0xAC03
#define GATTS_CHAR_UUID_PRO_KETNUOC 0xAC04
#define GATTS_CHAR_UUID_PRO_NUOCVAO 0xAC05
#define GATTS_CHAR_UUID_PRO_LTIME 0xAC06
#define GATTS_CHAR_UUID_PRO_HSD 0xAC07
#define GATTS_CHAR_UUID_PRO_RUNTIMETOTAL 0xAC08
#define GATTS_CHAR_UUID_PRO_RUNTIMEREMAIN 0xAC09
#define GATTS_CHAR_UUID_PRO_TDS 0xAC0A
#define GATTS_CHAR_UUID_PRO_ERROR 0xAC0B
#define GATTS_CHAR_UUID_PRO_VANXA 0xAC0C
#define GATTS_CHAR_UUID_PRO_TEMP_HOT 0xAC0D
#define GATTS_CHAR_UUID_PRO_TEMP_COLD 0xAC0E
#define GATTS_CHAR_UUID_PRO_CTRL_DISP 0xAC10
#define GATTS_CHAR_UUID_PRO_CTRL_HOT 0xAC11
#define GATTS_CHAR_UUID_PRO_CTRL_COLD 0xAC12

#define MAX_LEN_PROPERTY_CODE 20
#define PROPERTY_CODE_BOM "iro_bom"
#define PROPERTY_CODE_VANXA "iro_vx"
#define PROPERTY_CODE_KETNUOC "iro_ketn"
#define PROPERTY_CODE_NUOCVAO "iro_nuocIn"
#define PROPERTY_CODE_LTIME "iro_lTime"
#define PROPERTY_CODE_HSD "iro_hsd"
#define PROPERTY_CODE_RUNTIMETOTAL "iro_rtt"
#define PROPERTY_CODE_RUNTIMEREMAIN "iro_rtr"
#define PROPERTY_CODE_RESETFILTER "iro_rsft"
#define PROPERTY_CODE_TDS "iro_tds"
#define PROPERTY_CODE_ERROR "iro_errs"

#define PROPERTY_CODE_TEMP_HOT "temp_hot"
#define PROPERTY_CODE_TEMP_COLD "temp_cold"
#define PROPERTY_CODE_CTRL_DISP "ctrl_disp"
#define PROPERTY_CODE_CTRL_HOT "ctrl_hot"
#define PROPERTY_CODE_CTRL_COLD "ctrl_cold"

#define PROPERTY_CODE_FACTORY_RESET "FACTORY_RESET"

#define GATTS_DESCR_UUID_TEST_A 0x3333
#define GATTS_NUM_HANDLE_TEST_A 10

#define GATTS_DESCR_UUID_TEST_B 0x2222
#define GATTS_NUM_HANDLE_TEST_B 36

#define TEST_DEVICE_NAME DEVICE_NAME
#define TEST_MANUFACTURER_DATA_LEN 17

#define WIFI_LIST_CHAR_VAL_LEN_MAX 500
#define BLE_CONTROL_CHAR_VAL_LEN_MAX 500
#define COM_CHAR_VAL_MAX_LEN 100
#define DEVICE_INFO_CHAR_VAL_LEN_MAX 100

#define MAX_LEN_IO_CHAR 50
#define MAX_LEN_FILTER_CHAR 100

#define PREPARE_BUF_MAX_SIZE 1024

#define PROFILE_NUM 2
#define PROFILE_A_APP_ID 0
#define PROFILE_BLE_CONTROL_APP_ID 1

#define PRE_CMD_USE_WIFI "_UWF:"
#define PRE_CMD_USER_ID "_UID:"
#define PRE_CMD_LOCATION "_LOI:"
#define PRE_CMD_END "_END:"

#define PRE_CMD_USE_BLE "_UBL:"

#define END_OF_BLE_CONTROL_CMD 0x04
#define BEGIN_OF_BLE_CONTROL_CMD 0X01

#define END_OF_CMD 0x04
#define BEGIN_OF_CMD '_'
#define LEN_OF_PRE_CMD 5
#define PARAM_SEPARATE 0x06
#define START_PARAM_CHAR ':'
#define MAX_PARAM_NUM 10
typedef struct
{
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
} gatts_char_inst;

struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    gatts_char_inst charInsts[18];
    uint8_t numberOfChar;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

typedef struct
{
    uint8_t *prepare_buf;
    int prepare_len;
} prepare_type_env_t;

uint32_t lastWriteTranID;
bool registeredConfigService = false;
extern char g_product_Id[PRODUCT_ID_LEN];
extern bool g_enTest;
bool reAdv = false;
typedef enum
{
    CHAR_INST_ID_BLEC_COM = 0,
    CHAR_INST_ID_BLEC_VANXA,
    CHAR_INST_ID_BLEC_BOM,
    CHAR_INST_ID_BLEC_KETNUOC,
    CHAR_INST_ID_BLEC_NUOCVAO,
    CHAR_INST_ID_BLEC_LTIME,
    CHAR_INST_ID_BLEC_HSD,
    CHAR_INST_ID_BLEC_RUNTIMETOTAL,
    CHAR_INST_ID_BLEC_RUNTIMEREMAIN,
    CHAR_INST_ID_BLEC_TDS,
    CHAR_INST_ID_BLEC_ERROR,
    CHAR_INST_ID_GET_WATER,

    CHAR_INST_ID_BLEC_TEMP_HOT,
    CHAR_INST_ID_BLEC_TEMP_COLD,
    CHAR_INST_ID_BLEC_CTRL_DISP,
    CHAR_INST_ID_BLEC_CTRL_HOT,
    CHAR_INST_ID_BLEC_CTRL_COLD,

    CHAR_INST_ID_NUM,
} CharInstId_t;

typedef struct bleProChar
{
    CharInstId_t instId;
    uint16_t uuid;
    esp_attr_value_t *val;
    char proCode[MAX_LEN_PROPERTY_CODE];
} BleProChar_t;

esp_bd_addr_t cur_bda;  
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void startBLE();
static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gatts_profile_ble_control_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

void processCmd(uint8_t *cmdLine, uint16_t cmdLineLen);

void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
void writeToComCharEvent(uint8_t len, uint8_t *data);
void writeToBleControlComCharEvent(uint8_t len, uint8_t *data);
void writeToGetWaterCharEvt(uint8_t len, uint8_t *data);

void BLE_setNewListWifi(uint8_t* list, uint16_t len);
/*******************************************************************************
* Variables
******************************************************************************/
bool s_isBleControlMode = false;
bool g_bleConfigConnected = false;
bool s_bleControlConnected = false;

ComMode_t g_comMode = COM_MODE_WIFI;
char g_new_ssid[32], g_new_pwd[32];
bool g_haveNewSsid = false;
uint8_t wifi_list_char_str[WIFI_LIST_CHAR_VAL_LEN_MAX] = "no data";
esp_attr_value_t wifi_list_char_val =
    {
        .attr_max_len = WIFI_LIST_CHAR_VAL_LEN_MAX,
        .attr_len = sizeof(wifi_list_char_str),
        .attr_value = wifi_list_char_str,
};

const uint8_t device_info_char_str[] = "{\"ble\":1,\"deviceTypeName\":\"iro_v1\"}";
esp_attr_value_t device_info_char_val =
    {
        .attr_max_len = DEVICE_INFO_CHAR_VAL_LEN_MAX,
        .attr_len = strlen((const char *)device_info_char_str),
        .attr_value = device_info_char_str,
};

uint8_t blecontrol_char_str[BLE_CONTROL_CHAR_VAL_LEN_MAX] = "no data";
esp_attr_value_t bleControl_char_val =
    {
        .attr_max_len = BLE_CONTROL_CHAR_VAL_LEN_MAX,
        .attr_len = sizeof(blecontrol_char_str),
        .attr_value = blecontrol_char_str,
};

uint8_t getwater_char_str[BLE_CONTROL_CHAR_VAL_LEN_MAX] = "no data";
esp_attr_value_t getwater_char_val =
    {
        .attr_max_len = BLE_CONTROL_CHAR_VAL_LEN_MAX,
        .attr_len = sizeof(getwater_char_str),
        .attr_value = getwater_char_str,
};

uint8_t com_str[COM_CHAR_VAL_MAX_LEN] = "no data";
esp_attr_value_t com_char_val =
    {
        .attr_max_len = COM_CHAR_VAL_MAX_LEN,
        .attr_len = COM_CHAR_VAL_MAX_LEN,
        .attr_value = com_str,
};

uint8_t pro_vanxa_char_str[MAX_LEN_IO_CHAR] = "no data";
esp_attr_value_t pro_vanxa_char_val =
    {
        .attr_max_len = MAX_LEN_IO_CHAR,
        .attr_len = sizeof(pro_vanxa_char_str),
        .attr_value = pro_vanxa_char_str,
};
uint8_t pro_bom_char_str[MAX_LEN_IO_CHAR] = "no data";
esp_attr_value_t pro_bom_char_val =
    {
        .attr_max_len = MAX_LEN_IO_CHAR,
        .attr_len = sizeof(pro_bom_char_str),
        .attr_value = pro_bom_char_str,
};
uint8_t pro_ketnuoc_char_str[MAX_LEN_IO_CHAR] = "no data";
esp_attr_value_t pro_ketnuoc_char_val =
    {
        .attr_max_len = MAX_LEN_IO_CHAR,
        .attr_len = sizeof(pro_ketnuoc_char_str),
        .attr_value = pro_ketnuoc_char_str,
};

uint8_t pro_nuocvao_char_str[MAX_LEN_IO_CHAR] = "no data";
esp_attr_value_t pro_nuocvao_char_val =
    {
        .attr_max_len = MAX_LEN_IO_CHAR,
        .attr_len = sizeof(pro_nuocvao_char_str),
        .attr_value = pro_nuocvao_char_str,
};

uint8_t pro_tds_char_str[MAX_LEN_IO_CHAR] = "no data";
esp_attr_value_t pro_tds_char_val =
    {
        .attr_max_len = MAX_LEN_IO_CHAR,
        .attr_len = sizeof(pro_tds_char_str),
        .attr_value = pro_tds_char_str,
};
uint8_t pro_error_char_str[MAX_LEN_IO_CHAR] = "no data";
esp_attr_value_t pro_error_char_val =
    {
        .attr_max_len = MAX_LEN_IO_CHAR,
        .attr_len = sizeof(pro_error_char_str),
        .attr_value = pro_error_char_str,
};

uint8_t pro_lTime_char_str[MAX_LEN_FILTER_CHAR] = "no data";
esp_attr_value_t pro_lTime_char_val =
    {
        .attr_max_len = MAX_LEN_FILTER_CHAR,
        .attr_len = sizeof(pro_lTime_char_str),
        .attr_value = pro_lTime_char_str,
};

uint8_t pro_hsd_char_str[MAX_LEN_FILTER_CHAR] = "no data";
esp_attr_value_t pro_hsd_char_val =
    {
        .attr_max_len = MAX_LEN_FILTER_CHAR,
        .attr_len = sizeof(pro_hsd_char_str),
        .attr_value = pro_hsd_char_str,
};
uint8_t pro_runtimetotal_char_str[MAX_LEN_FILTER_CHAR] = "no data";
esp_attr_value_t pro_runtimetotal_char_val =
    {
        .attr_max_len = MAX_LEN_FILTER_CHAR,
        .attr_len = sizeof(pro_runtimetotal_char_str),
        .attr_value = pro_runtimetotal_char_str,
};
uint8_t pro_runtimeRemain_char_str[MAX_LEN_FILTER_CHAR] = "no data";
esp_attr_value_t pro_runtimeRemain_char_val =
    {
        .attr_max_len = MAX_LEN_FILTER_CHAR,
        .attr_len = sizeof(pro_runtimeRemain_char_str),
        .attr_value = pro_runtimeRemain_char_str,
};
uint8_t pro_tempHot_char_str[MAX_LEN_IO_CHAR] = "no data";
esp_attr_value_t pro_temHot_char_val =
    {
        .attr_max_len = MAX_LEN_IO_CHAR,
        .attr_len = sizeof(pro_tempHot_char_str),
        .attr_value = pro_tempHot_char_str,
};
uint8_t pro_tempCold_char_str[MAX_LEN_IO_CHAR] = "no data";
esp_attr_value_t pro_tempCold_char_val =
    {
        .attr_max_len = MAX_LEN_IO_CHAR,
        .attr_len = sizeof(pro_tempCold_char_str),
        .attr_value = pro_tempCold_char_str,
};
uint8_t pro_ctrlDisp_char_str[MAX_LEN_IO_CHAR] = "no data";
esp_attr_value_t pro_ctrlDisp_char_val =
    {
        .attr_max_len = MAX_LEN_IO_CHAR,
        .attr_len = sizeof(pro_ctrlDisp_char_str),
        .attr_value = pro_ctrlDisp_char_str,
};
uint8_t pro_ctrlHot_char_str[MAX_LEN_IO_CHAR] = "no data";
esp_attr_value_t pro_ctrlHot_char_val =
    {
        .attr_max_len = MAX_LEN_IO_CHAR,
        .attr_len = sizeof(pro_ctrlHot_char_str),
        .attr_value = pro_ctrlHot_char_str,
};
uint8_t pro_ctrlCold_char_str[MAX_LEN_IO_CHAR] = "no data";
esp_attr_value_t pro_ctrlCold_char_val =
    {
        .attr_max_len = MAX_LEN_IO_CHAR,
        .attr_len = sizeof(pro_ctrlCold_char_str),
        .attr_value = pro_ctrlCold_char_str,
};
#define NUM_OF_PRO 15
const BleProChar_t proChars[NUM_OF_PRO] =
    {
        {
            .instId = CHAR_INST_ID_BLEC_VANXA,
            .uuid = GATTS_CHAR_UUID_PRO_VANXA,
            .val = &pro_vanxa_char_val,
            .proCode = PROPERTY_CODE_VANXA,
        },
        {
            .instId = CHAR_INST_ID_BLEC_BOM,
            .uuid = GATTS_CHAR_UUID_PRO_BOM,
            .val = &pro_bom_char_val,
            .proCode = PROPERTY_CODE_BOM,
        },
        {
            .instId = CHAR_INST_ID_BLEC_KETNUOC,
            .uuid = GATTS_CHAR_UUID_PRO_KETNUOC,
            .val = &pro_ketnuoc_char_val,
            .proCode = PROPERTY_CODE_KETNUOC,
        },
        {
            .instId = CHAR_INST_ID_BLEC_NUOCVAO,
            .uuid = GATTS_CHAR_UUID_PRO_NUOCVAO,
            .val = &pro_nuocvao_char_val,
            .proCode = PROPERTY_CODE_NUOCVAO,
        },
        {
            .instId = CHAR_INST_ID_BLEC_LTIME,
            .uuid = GATTS_CHAR_UUID_PRO_LTIME,
            .val = &pro_lTime_char_val,
            .proCode = PROPERTY_CODE_LTIME,
        },
        {
            .instId = CHAR_INST_ID_BLEC_HSD,
            .uuid = GATTS_CHAR_UUID_PRO_HSD,
            .val = &pro_hsd_char_val,
            .proCode = PROPERTY_CODE_HSD,
        },
        {
            .instId = CHAR_INST_ID_BLEC_RUNTIMETOTAL,
            .uuid = GATTS_CHAR_UUID_PRO_RUNTIMETOTAL,
            .val = &pro_runtimetotal_char_val,
            .proCode = PROPERTY_CODE_RUNTIMETOTAL,
        },
        {
            .instId = CHAR_INST_ID_BLEC_RUNTIMEREMAIN,
            .uuid = GATTS_CHAR_UUID_PRO_RUNTIMEREMAIN,
            .val = &pro_runtimeRemain_char_val,
            .proCode = PROPERTY_CODE_RUNTIMEREMAIN,
        },
        {
            .instId = CHAR_INST_ID_BLEC_TDS,
            .uuid = GATTS_CHAR_UUID_PRO_TDS,
            .val = &pro_tds_char_val,
            .proCode = PROPERTY_CODE_TDS,
        },
        {
            .instId = CHAR_INST_ID_BLEC_ERROR,
            .uuid = GATTS_CHAR_UUID_PRO_ERROR,
            .val = &pro_error_char_val,
            .proCode = PROPERTY_CODE_ERROR,
        },
        {
            .instId = CHAR_INST_ID_BLEC_TEMP_HOT,
            .uuid = GATTS_CHAR_UUID_PRO_TEMP_HOT,
            .val = &pro_temHot_char_val,
            .proCode = PROPERTY_CODE_TEMP_HOT,
        },
        {
            .instId = CHAR_INST_ID_BLEC_TEMP_COLD,
            .uuid = GATTS_CHAR_UUID_PRO_TEMP_COLD,
            .val = &pro_tempCold_char_val,
            .proCode = PROPERTY_CODE_TEMP_COLD,
        },
        {
            .instId = CHAR_INST_ID_BLEC_CTRL_DISP,
            .uuid = GATTS_CHAR_UUID_PRO_CTRL_DISP,
            .val = &pro_ctrlDisp_char_val,
            .proCode = PROPERTY_CODE_CTRL_DISP,
        },
        {
            .instId = CHAR_INST_ID_BLEC_CTRL_HOT,
            .uuid = GATTS_CHAR_UUID_PRO_CTRL_HOT,
            .val = &pro_ctrlHot_char_val,
            .proCode = PROPERTY_CODE_CTRL_HOT,
        },
        {
            .instId = CHAR_INST_ID_BLEC_CTRL_COLD,
            .uuid = GATTS_CHAR_UUID_PRO_CTRL_COLD,
            .val = &pro_ctrlCold_char_val,
            .proCode = PROPERTY_CODE_CTRL_COLD,
        }};
#ifdef CONFIG_SET_RAW_ADV_DATA
static uint8_t raw_adv_data[] = {
    0x02, 0x01, 0x06,
    0x02, 0x0a, 0xeb, 0x03, 0x03, 0xab, 0xcd};
static uint8_t raw_scan_rsp_data[] = {
    0x0f, 0x09, 0x45, 0x53, 0x50, 0x5f, 0x47, 0x41, 0x54, 0x54, 0x53, 0x5f, 0x44,
    0x45, 0x4d, 0x4f};
#else
static uint8_t config_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAB, 0x12, 0x00, 0x00,
    //second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAC, 0x12, 0x00, 0x00};
static uint8_t ble_control_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAC, 0x12, 0x00, 0x00,
    //second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAB, 0x02, 0x00, 0x00};

//static uint8_t test_manufacturer[TEST_MANUFACTURER_DATA_LEN] =  {0x12, 0x23, 0x45, 0x56};
static esp_ble_adv_data_t config_adv_data = {
    set_scan_rsp : false,
    include_name : true,
    include_txpower : true,
    min_interval : 0x20,
    max_interval : 0x40,
    appearance : 0x00,
    manufacturer_len : 0,       //TEST_MANUFACTURER_DATA_LEN,
    p_manufacturer_data : NULL, //&test_manufacturer[0],
    service_data_len : 0,
    p_service_data : NULL,
    service_uuid_len : 32,
    p_service_uuid : config_service_uuid128,
    flag : (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t bleControl_adv_data = {
    set_scan_rsp : false,
    include_name : true,
    include_txpower : true,
    min_interval : 0x20,
    max_interval : 0x40,
    appearance : 0x00,
    manufacturer_len : 0,       //TEST_MANUFACTURER_DATA_LEN,
    p_manufacturer_data : NULL, //&test_manufacturer[0],
    service_data_len : 0,
    p_service_data : NULL,
    service_uuid_len : 16,
    p_service_uuid : ble_control_service_uuid128,
    flag : (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

#endif /* CONFIG_SET_RAW_ADV_DATA */

static esp_ble_adv_params_t test_adv_params;

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gatts_cb = gatts_profile_a_event_handler,
        .gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
    [PROFILE_BLE_CONTROL_APP_ID] = {
        .gatts_cb = gatts_profile_ble_control_event_handler, /* This demo does not implement, similar as profile A */
        .gatts_if = ESP_GATT_IF_NONE,                        /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};
static prepare_type_env_t a_prepare_write_env;
static prepare_type_env_t b_prepare_write_env;

/*******************************************************************************
 * Code
 ******************************************************************************/

/************************************************
 * BLE event
 ************************************************/

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        if(param->adv_data_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            log_info("set adv successfully\n");
            esp_ble_gap_start_advertising(&test_adv_params);
        }
        else{
            log_error("set adv fail\n");
        }
        break;
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&test_adv_params);
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&test_adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            log_error("Advertising start failed\n");
        } 
        else{
            log_info("Advertising start ok\n");
            if(g_enTest && reAdv)
            {
                reAdv = false;
                char buf[50];
                sprintf(buf,"[BLE_NAME,TCM-%s,]",g_product_Id);
                UART_sendRightNow(buf);
            }
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            log_error("Advertising stop failed\n");
        }
        else
        {
            log_info("Stop adv successfully\n");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        log_info("update connetion params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC)
    {
        esp_log_buffer_hex(GATTS_TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
    }
    else
    {
        log_info("ESP_GATT_PREP_WRITE_CANCEL");
    }
    if (prepare_write_env->prepare_buf)
    {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        log_info("A:REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);
        gl_profile_tab[PROFILE_A_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_A;
        //esp_ble_gap_set_device_name(TEST_DEVICE_NAME);

#ifdef CONFIG_SET_RAW_ADV_DATA
        esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
        esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
#else
        esp_ble_gap_config_adv_data(&config_adv_data);
#endif
        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_A_APP_ID].service_id, GATTS_NUM_HANDLE_TEST_A);
        break;
    case ESP_GATTS_READ_EVT:
    {
        log_info("A:GATT_READ_EVT");
        break;
    }
    case ESP_GATTS_WRITE_EVT:
    {
        // log_info( "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d\n", param->write.conn_id, param->write.trans_id, param->write.handle);
        log_info("A:GATT_WRITE_EVT, value len %d, value %s\n", param->write.len, (char *)param->write.value);

        if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].charInsts[1].char_handle)
        {
            writeToComCharEvent(param->write.len, param->write.value);
        }
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        log_info("A:ESP_GATTS_EXEC_WRITE_EVT");
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        example_exec_write_event_env(&a_prepare_write_env, param);

        break;
    case ESP_GATTS_MTU_EVT:
    case ESP_GATTS_CONF_EVT:
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
        log_info("A:CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;
        gl_profile_tab[PROFILE_A_APP_ID].charInsts[0].char_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].charInsts[0].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_WIFI_LIST;

        gl_profile_tab[PROFILE_A_APP_ID].charInsts[1].char_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].charInsts[1].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_COM;

        gl_profile_tab[PROFILE_A_APP_ID].charInsts[2].char_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].charInsts[2].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_DEVICE_INFO;

        gl_profile_tab[PROFILE_A_APP_ID].numberOfChar = 3;

        esp_ble_gatts_start_service(gl_profile_tab[PROFILE_A_APP_ID].service_handle);

        esp_attr_control_t testAttrControl;
        testAttrControl.auto_rsp = ESP_GATT_AUTO_RSP;
        // add wifi list char
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &gl_profile_tab[PROFILE_A_APP_ID].charInsts[0].char_uuid,
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               &wifi_list_char_val, &testAttrControl);
        // add com char
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &gl_profile_tab[PROFILE_A_APP_ID].charInsts[1].char_uuid,
                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               &com_char_val, &testAttrControl);
        // add descriptor for com char
        // gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
        // gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
        // esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &gl_profile_tab[PROFILE_A_APP_ID].descr_uuid,
        //                                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        // add device info char
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &gl_profile_tab[PROFILE_A_APP_ID].charInsts[2].char_uuid,
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               &device_info_char_val, &testAttrControl);
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
    {
        uint16_t length = 0;
        const uint8_t *prf_char;

        log_info("A:ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d\n",
                 param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        if (param->add_char.status)
            break;
        for (int i = 0; i < gl_profile_tab[PROFILE_A_APP_ID].numberOfChar; i++)
        {
            if (param->add_char.char_uuid.uuid.uuid16 == gl_profile_tab[PROFILE_A_APP_ID].charInsts[i].char_uuid.uuid.uuid16)
            {
                gl_profile_tab[PROFILE_A_APP_ID].charInsts[i].char_handle = param->add_char.attr_handle;
                if (gl_profile_tab[PROFILE_A_APP_ID].charInsts[i].char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_COM)
                {
                    // gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
                    // gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
                    // esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &gl_profile_tab[PROFILE_A_APP_ID].descr_uuid,
                    //                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
                }
                 if(i == (gl_profile_tab[PROFILE_A_APP_ID].numberOfChar -1))
                {
                    if(g_enTest)
                    {
                        char name[35] = "TCM-";
                        strcat(name, g_product_Id);
                        char buf[60] = "";
                        sprintf(buf,"[BLE_NAME,%s,]",name);
                        UART_sendRightNow(buf);
                    }
                }
            }
        }

        

        esp_ble_gatts_get_attr_value(param->add_char.attr_handle, &length, &prf_char);
        
        log_info("A:the gatts demo char length = %x\n", length);
        // for (int i = 0; i < length; i++) {
        //     log_info( "prf_char[%x] =%x\n", i, prf_char[i]);
        // }
        // esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &gl_profile_tab[PROFILE_A_APP_ID].descr_uuid,
        //                              ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        log_info("A:ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
                 param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        log_info("A:SERVICE_START_EVT, status %d, service_handle %d\n",
                 param->start.status, param->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT:
    {
        if(s_isBleControlMode && !isWifiCofg) break;
        esp_ble_conn_update_params_t conn_params;
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x50; // max_int = 0x50*1.25ms = 100ms
        conn_params.min_int = 0x30; // min_int = 0x30*1.25ms = 60ms
        conn_params.timeout = 400;  // timeout = 400*10ms = 4000ms
        log_info("A: ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:, is_conn \n",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = param->connect.conn_id;
        //start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);

        g_bleConfigConnected = true;
        if(isWifiCofg)
        {
            UART_SendLedBlinkCmd(LED_BLINK_STATE_BLINK,2000);
        }
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        log_info("A: ESP_GATTS_DISCONNECT_EVT");
        g_bleConfigConnected = false;
        if(!isWifiCofg) break;
        esp_ble_gap_start_advertising(&test_adv_params);
        if(isWifiCofg)
        {
            UART_SendLedBlinkCmd(LED_BLINK_STATE_BLINK,200);
        }
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

static void gatts_profile_ble_control_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "B:REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);
        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_BLE_CONTROL;

        esp_ble_gap_config_adv_data(&bleControl_adv_data);

        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_id, GATTS_NUM_HANDLE_TEST_B);
        break;
    case ESP_GATTS_READ_EVT:
    {
        log_info("B:GATT_READ_EVT");
        break;
    }
    case ESP_GATTS_WRITE_EVT:
    {
        log_info("B:GATT_WRITE_EVT, value len %d, value %s\n", param->write.len, (char *)param->write.value);

        if (param->write.handle == gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[CHAR_INST_ID_BLEC_COM].char_handle)
        {
            writeToBleControlComCharEvent(param->write.len, param->write.value);
        }
        else if (param->write.handle == gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[CHAR_INST_ID_GET_WATER].char_handle)
        {
            writeToGetWaterCharEvt(param->write.len, param->write.value);
        }
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGI(GATTS_TAG, "B:ESP_GATTS_EXEC_WRITE_EVT");
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        example_exec_write_event_env(&b_prepare_write_env, param);
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(GATTS_TAG, "B:ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:

        ESP_LOGI(GATTS_TAG, "B:CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_handle = param->create.service_handle;
        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].numberOfChar = CHAR_INST_ID_NUM;

        esp_ble_gatts_start_service(gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_handle);
        esp_attr_control_t testAttrControl;
        testAttrControl.auto_rsp = ESP_GATT_AUTO_RSP;
        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[CHAR_INST_ID_BLEC_COM].char_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[CHAR_INST_ID_BLEC_COM].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_BLE_CONTROL_COM;
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_handle, &gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[CHAR_INST_ID_BLEC_COM].char_uuid,
                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                               &bleControl_char_val, &testAttrControl);

        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[CHAR_INST_ID_GET_WATER].char_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[CHAR_INST_ID_GET_WATER].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_BLE_GET_WATER;
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_handle, &gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[CHAR_INST_ID_GET_WATER].char_uuid,
                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_WRITE_NR,
                               &getwater_char_val, &testAttrControl);

        for (uint8_t i = 0; i < NUM_OF_PRO; i++)
        {
            gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[proChars[i].instId].char_uuid.len = ESP_UUID_LEN_16;
            gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[proChars[i].instId].char_uuid.uuid.uuid16 = proChars[i].uuid;
            esp_ble_gatts_add_char(gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].service_handle, &gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[proChars[i].instId].char_uuid,
                                   ESP_GATT_PERM_READ,
                                   ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                   proChars[i].val, &testAttrControl);
        }
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(GATTS_TAG, "B:ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d\n",
                 param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        if (param->add_char.status)
            break;
        int i;
        for (i = 0; i < gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].numberOfChar; ++i)
        {
            if (param->add_char.char_uuid.uuid.uuid16 == gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[i].char_uuid.uuid.uuid16)
            {
                gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[i].char_handle = param->add_char.attr_handle;
                if(i == (gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].numberOfChar -1))
                {
                    printf("init ble control done!\n");
                    UART_needUpdateAllParam();
                }
            }
        }
        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].descr_handle = param->add_char_descr.attr_handle;
        ESP_LOGI(GATTS_TAG, "B:ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
                 param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(GATTS_TAG, "B:SERVICE_START_EVT, status %d, service_handle %d\n",
                 param->start.status, param->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT:
        if(isWifiCofg) break;
        ESP_LOGI(GATTS_TAG, "B:CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        esp_ble_conn_update_params_t conn_params;
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        memcpy(cur_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 24; // max_int = 0x50*1.25ms = 100ms
        conn_params.min_int = 12; // min_int = 0x30*1.25ms = 60ms
        conn_params.timeout = 400;  // timeout = 400*10ms = 4000ms
        //start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);

        gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].conn_id = param->connect.conn_id;
        // if (g_comMode == COM_MODE_BLE)
        // {
            s_bleControlConnected = true;
        // }
        break;
    case ESP_GATTS_CONF_EVT:
        ESP_LOGI(GATTS_TAG, "B:ESP_GATTS_CONF_EVT status %d attr_handle %d", param->conf.status, param->conf.handle);
        if (param->conf.status != ESP_GATT_OK)
        {
            esp_log_buffer_hex(GATTS_TAG, param->conf.value, param->conf.len);
        }
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        s_bleControlConnected = false;
        log_info("B: ESP_GATTS_DISCONNECT_EVT");
        if(isWifiCofg) break;
        esp_ble_gap_start_advertising(&test_adv_params);
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        }
        else
        {
            log_info("Reg app failed, app_id %04x, status %d\n",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }

    /* If the gatts_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do
    {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++)
        {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                gatts_if == gl_profile_tab[idx].gatts_if)
            {
                if (gl_profile_tab[idx].gatts_cb)
                {
                    gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

/************************************************
 * Application functions
 ************************************************/

bool inited = false;

void BLE_init()
{
    log_info(" init bluetooth \n");
    test_adv_params.adv_int_min = 0x20;
    test_adv_params.adv_int_max = 0x40;
    test_adv_params.adv_type = ADV_TYPE_IND;
    test_adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    test_adv_params.channel_map = ADV_CHNL_ALL;
    test_adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    // gl_profile_tab[PROFILE_A_APP_ID].gatts_cb = gatts_profile_a_event_handler;
    // gl_profile_tab[PROFILE_A_APP_ID].gatts_if = ESP_GATT_IF_NONE;
    // gl_profile_tab[PROFILE_A_APP_ID].numberOfChar = 0;

    esp_err_t ret;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        log_error("%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        log_error("%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_init();
    if (ret)
    {
        log_error("%s init bluetooth failed\n", __func__);
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret)
    {
        log_error("%s enable bluetooth failed\n", __func__);
        return;
    }

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret)
    {
        ESP_LOGE(GATTS_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    char name[35] = "TCM-";
    strcat(name, g_product_Id);
    printf(name);
    esp_ble_gap_set_device_name(name);
    inited = true;
}
void BLE_start(uint8_t *wifiList, uint16_t wifiListLen)
{
    if (!inited)
    {
        BLE_init();
    }
    
    if(s_bleControlConnected)
    {
        esp_ble_gap_disconnect(cur_bda);
    }
    if(!registeredConfigService){
        memcpy(wifi_list_char_str, wifiList, wifiListLen);
        wifi_list_char_val.attr_len = wifiListLen;
        esp_ble_gatts_app_register(PROFILE_A_APP_ID);
        registeredConfigService = true;
    }
    else{
        BLE_setNewListWifi(wifiList, wifiListLen);
        printf("change advertising data config wifi\n");
        esp_ble_gap_config_adv_data(&config_adv_data);
    }
    return;
}

void BLE_StopBLE()
{
    log_info("Stop ble");
    esp_err_t ret;
    esp_ble_gatts_close(gl_profile_tab[PROFILE_A_APP_ID].gatts_if, gl_profile_tab[PROFILE_A_APP_ID].conn_id);
    // esp_ble_gatts_delete_service(gl_profile_tab[PROFILE_A_APP_ID].service_handle );
    // ret = esp_ble_gatts_app_unregister(gl_profile_tab[PROFILE_A_APP_ID].gatts_if);
    // if (ret) {
    //     log_error( "%s esp_ble_gatts_app_unregister failed\n", __func__);
    //     return;
    // }
    // log_info( "esp_bluedroid_disable");
    ret = esp_bluedroid_disable();
    log_info("esp_bluedroid_disable done");
    if (ret)
    {
        log_error(" esp_bluedroid_disable failed\n");
        return;
    }
    log_info("esp_bluedroid_deinit");
    ret = esp_bluedroid_deinit();
    if (ret)
    {
        log_error("%s esp_bluedroid_deinit failed\n", __func__);
        return;
    }
    // ret = esp_bt_controller_disable();
    // if (ret) {
    //     log_error( "%s esp_bt_controller_disable failed\n", __func__);
    //     return;
    // }
    // log_info( "esp_bt_controller_deinit");
    // ret = esp_bt_controller_deinit();
    // if (ret) {
    //     log_error( "%s esp_bt_controller_deinit failed\n", __func__);
    //     return;
    // }
}
void BLE_releaseBle()
{
    if(!inited) return;
    inited = false;
    s_bleControlConnected = false;
    g_bleConfigConnected = false;
    registeredConfigService = false;
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    esp_bt_mem_release(ESP_BT_MODE_BTDM);
    printf("release ble done\n");
}

void BLE_getMesParamIndex(uint8_t *cmdLine, uint16_t cmdLineLen, uint16_t *indexList, uint8_t *indexNum)
{
    for (uint8_t i = 0; i < cmdLineLen; i++)
    {
        if ((cmdLine[i] == START_PARAM_CHAR) && (*indexNum == 0))
        {
            indexList[*indexNum] = i + 1;
            *indexNum += 1;
        }
        if ((cmdLine[i] == PARAM_SEPARATE) && (*indexNum > 0))
        {
            indexList[*indexNum] = i + 1;
            *indexNum += 1;
            if (*indexNum >= MAX_PARAM_NUM)
            {
                return;
            }
        }
    }
}

void BLE_startBleControlMode()
{
    if (!s_isBleControlMode)
    {
        if (!inited)
        {
            BLE_init();
        }
        esp_ble_gatts_app_register(PROFILE_BLE_CONTROL_APP_ID);
        s_isBleControlMode = true;
    }
    else{
        if(!inited) return;
        printf("change advertising data BLE\n");
        esp_ble_gap_config_adv_data(&bleControl_adv_data);
    }
}
void processCmd(uint8_t *cmdLine, uint16_t cmdLineLen)
{
    log_info("....new command line \n");
    uint16_t paramIndexList[MAX_PARAM_NUM];
    uint8_t paramIndexNum = 0;
    BLE_getMesParamIndex(cmdLine, cmdLineLen, paramIndexList, &paramIndexNum);
    if (paramIndexNum == 0)
    {
        log_error("command have no param");
        return;
    }
    char cmdTypeStr[LEN_OF_PRE_CMD + 1];
    if (paramIndexList[0] > (LEN_OF_PRE_CMD + 1))
    {
        log_error("command type str too long");
        return;
    }
    memcpy(cmdTypeStr, cmdLine, paramIndexList[0]);
    cmdTypeStr[paramIndexList[0]] = 0;
    if (strncmp(PRE_CMD_USE_WIFI, cmdTypeStr, paramIndexList[0]) == 0)
    {
        if (paramIndexNum != 2)
        {
            log_error("wifi password message, syntax error");
            return;
        }
        memcpy(g_new_ssid, cmdLine + paramIndexList[0], (paramIndexList[1] - paramIndexList[0] - 1));
        g_new_ssid[paramIndexList[1] - paramIndexList[0] - 1] = '\0';
        memcpy(g_new_pwd, cmdLine + paramIndexList[1], (cmdLineLen - paramIndexList[1]));
        g_new_pwd[cmdLineLen - paramIndexList[1]] = '\0';
        if(isWifiCofg)
        {
            g_haveNewSsid = true;
            g_comMode = COM_MODE_WIFI;
            FlashHandler_saveComMode();
        }
    }
    else if (strncmp(PRE_CMD_USER_ID, cmdTypeStr, paramIndexList[0]) == 0)
    {
        if (paramIndexNum != 1)
        {
            log_error("user id message, syntax error");
            return;
        }
        char userId[100];
        memcpy(userId, cmdLine + paramIndexList[0], (cmdLineLen - paramIndexList[0]));
        userId[cmdLineLen - paramIndexList[0]] = '\0';
        haveUserInfo(userId);
    }
    else if (strncmp(PRE_CMD_LOCATION, cmdTypeStr, paramIndexList[0]) == 0)
    {
        if (paramIndexNum != 2)
        {
            log_error("locaiton message, syntax error");
            return;
        }
        char latStr[15], lonStr[15];
        memcpy(latStr, cmdLine + paramIndexList[0], (paramIndexList[1] - paramIndexList[0] - 1));
        latStr[paramIndexList[1] - paramIndexList[0] - 1] = '\0';
        memcpy(lonStr, cmdLine + paramIndexList[1], (cmdLineLen - paramIndexList[1]));
        lonStr[cmdLineLen - paramIndexList[1]] = '\0';
        log_info("new locaiton %s , %s", latStr, lonStr);
        Http_sendDeviceLocation(latStr, lonStr);
    }
    else if (strncmp(PRE_CMD_END, cmdTypeStr, paramIndexList[0]) == 0)
    {
        recievedBleDone();
    }
    else if (strncmp(PRE_CMD_USE_BLE, cmdTypeStr, paramIndexList[0]) == 0)
    {
        BLE_startBleControlMode();
        BLE_sentToMobile(BLE_MES_BLE_CONTROL_MODE_OK);
        g_comMode = COM_MODE_BLE;
        FlashHandler_saveComMode();
    }
}
void BLE_setPropertyData(const char *pcode, const char *pdata)
{
    if(!inited) return;
    esp_err_t errRc;
    for (uint8_t i = 0; i < NUM_OF_PRO; i++)
    {
        if (strcmp(pcode, proChars[i].proCode) == 0)    //ghi vao character tuong ung propertyCode
        {
            errRc = esp_ble_gatts_set_attr_value(gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[proChars[i].instId].char_handle, strlen(pdata), (uint8_t *)pdata);
            if (errRc != ESP_OK)
            {
                log_error("set attr value false  rc=%d %s", errRc, Utils_errorToString(errRc));
            }
            else{
                log_info("set attr value ok  %s", pcode);
            }
            char notifyStr[20];
            sprintf(notifyStr, "{\"id\":\"%04X\"}", proChars[i].uuid); //notify vao char com, noi dung chua id character can update
            BLE_sendToBleControlChar(notifyStr);        //notify cho client to update character
            return;
        }
    }
    log_error("property code not found: %s", pcode);
}
void BLE_setNewListWifi(uint8_t* list, uint16_t len)
{
    esp_err_t errRc;
    errRc = esp_ble_gatts_set_attr_value(gl_profile_tab[PROFILE_A_APP_ID].charInsts[0].char_handle, len, (uint8_t *)list);
    if (errRc != ESP_OK)
    {
        log_error("set attr list wifi value false  rc=%d %s", errRc, Utils_errorToString(errRc));
    }
    else{
        log_info("set attr list wifi value ok");
    }
}
void BLE_sentToMobile(const char *sentMes)
{
    if(!inited) return;
    esp_err_t errRc = esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_A_APP_ID].gatts_if, gl_profile_tab[PROFILE_A_APP_ID].conn_id, gl_profile_tab[PROFILE_A_APP_ID].charInsts[1].char_handle, strlen(sentMes), (uint8_t *)sentMes, false);

    if (errRc != ESP_OK)
    {
        log_error("<< esp_ble_gatts_send_indicate: rc=%d %s", errRc, Utils_errorToString(errRc));
    }
    else
    {
        log_info("sen notify ok: %s", sentMes);
    }
}

void BLE_sendToBleControlChar(const char *sentMes)
{
    if(!inited) return;
    if (!s_bleControlConnected && !g_bleConfigConnected)
    {
        return;
    }

    esp_err_t errRc = esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].gatts_if, gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].conn_id, gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[CHAR_INST_ID_BLEC_COM].char_handle, strlen(sentMes), (uint8_t *)sentMes, false);

    if (errRc != ESP_OK)
    {
        log_error("<< esp_ble_gatts_send_indicate: rc=%d %s", errRc, Utils_errorToString(errRc));
    }
    else
    {
        log_info("sen to ble control char ok:%s", sentMes);
    }
}

void BLE_sendToGetWaterChar(const char *sentMes)
{
    if (!s_bleControlConnected && !g_bleConfigConnected)
    {
        return;
    }

    esp_err_t errRc = esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].gatts_if, gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].conn_id, gl_profile_tab[PROFILE_BLE_CONTROL_APP_ID].charInsts[CHAR_INST_ID_GET_WATER].char_handle, strlen(sentMes), (uint8_t *)sentMes, false);

    if (errRc != ESP_OK)
    {
        log_error("<< esp_ble_gatts_send_indicate: rc=%d %s", errRc, Utils_errorToString(errRc));
    }
    else
    {
        log_info("sen to ble control char ok:%s", sentMes);
    }
}

void writeToComCharEvent(uint8_t len, uint8_t *data)
{
    // BLE_handler::sentToMobile("hello");
    static uint8_t cmdLine[100];
    static uint8_t cmdLineLen = 0;
    if (cmdLineLen == 0 && data[0] != BEGIN_OF_CMD)
    {
        return;
    }
    memcpy(cmdLine + cmdLineLen, data, len);
    cmdLineLen += len;
    log_info("data end: %x", data[len - 1]);
    if (data[len - 1] == END_OF_CMD)
    {
        processCmd(cmdLine, cmdLineLen - 1); // data không bao gồm ký tự kết thúc bản tin
        cmdLineLen = 0;
    }
}

void writeToBleControlComCharEvent(uint8_t len, uint8_t *data)
{
    // BLE_handler::sentToMobile("hello");
    static char cmdLine[200];
    static uint8_t cmdLineLen = 0;
    if (cmdLineLen == 0 && data[0] != BEGIN_OF_BLE_CONTROL_CMD)
    {
        return;
    }
    memcpy(cmdLine + cmdLineLen, data, len);
    cmdLineLen += len;
    if (data[len - 1] == END_OF_BLE_CONTROL_CMD)
    {
        cmdLine[cmdLineLen - 1] = '\0';
        log_info("Ble control message: %s", cmdLine + 1);
        BLEC_processMess(cmdLine + 1);
        cmdLineLen = 0;
    }
}

void writeToGetWaterCharEvt(uint8_t len, uint8_t *data)
{
    // BLE_handler::sentToMobile("hello");
    static char cmdLine[200];

    memcpy(cmdLine , data, len);
    cmdLine[len] = '\0';

        log_info("Ble getwater message: %s", cmdLine);
        BLEC_processGetwater(cmdLine);

}

void BLE_reAdvertising()
{ 
    if(!inited){
        printf("wait BLE init...\n");
        return;
    } 
    reAdv = true;
    esp_ble_gap_stop_advertising();
    vTaskDelay(1000/portTICK_PERIOD_MS);
    char name[35] = "TCM-";
    strcat(name, g_product_Id);
    printf(name);
    esp_ble_gap_set_device_name(name);
    esp_ble_gap_config_adv_data(&config_adv_data);
}
/***********************************************/
