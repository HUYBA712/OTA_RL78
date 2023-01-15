#ifndef MKP_W_PROTOCOL_H
#define MKP_W_PROTOCOL_H
#include "Global.h"
#include "cJSON.h"


/*protocol IRO*/
typedef enum
{
	IRO_VAN_XA = 0,
	IRO_BOM,
	IRO_KET_NUOC,
	IRO_NUOC_VAO,
	IRO_lifeTime,
	IRO_HSD,
	IRO_RUN_TIME_TOTAL,
	IRO_RUN_TIME_REMAIN,
	IRO_RESET_FILTER,
	IRO_TDS,
	IRO_pumpTime,
	IRO_ERRS,
	IRO_tempHot,
	IRO_tempCold,
	IRO_ctrlDisp,    //for khoi nong nhanh
	IRO_ctrlHot,    //for co dieu khien TS01
	IRO_ctrlCold,    //for co dieu khien TS01
	IRO_MAX_ATT
} attribute_name_t;

#define IRO_MAX_NUM_ATT IRO_MAX_ATT


typedef struct
{
	attribute_name_t att;
	const char property_code[20];
} iro_struct_att_to_wifi_t;


enum
{
	INDEX_UP,
	INDEX_PRO_CODE,
	INDEX_LEN_DATA
};


void PROTO_processCertificate(char* topic, char* data);
void PROTO_processCmd(char *data);
bool handleDataMsgFromUart(uint8_t* data_in, char* value_iro);
void PROTO_processUpdateProperty(cJSON *msgObject, const char *propertyCode);


#endif