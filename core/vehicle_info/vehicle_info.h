#ifndef VEHICLE_INFO_H
#define VEHICLE_INFO_H

#include "../types.h"
#include "../obd2/obd2.h"
#include "../error/error_handler.h"

#define VIN_LENGTH 17
#define CALIBRATION_ID_LENGTH 16
#define CVN_LENGTH 4
#define ECU_NAME_LENGTH 20
#define MAX_ECUS 8

typedef enum {
    VEHICLE_INFO_VIN_COUNT = 0x01,
    VEHICLE_INFO_VIN = 0x02,
    VEHICLE_INFO_CAL_ID_COUNT = 0x03,
    VEHICLE_INFO_CAL_ID = 0x04,
    VEHICLE_INFO_CVN_COUNT = 0x05,
    VEHICLE_INFO_CVN = 0x06,
    VEHICLE_INFO_IPT_COUNT = 0x07,
    VEHICLE_INFO_IPT = 0x08,
    VEHICLE_INFO_ECU_NAME = 0x0A,
    VEHICLE_INFO_MAX
} VehicleInfoType_t;

typedef struct {
    char vin[VIN_LENGTH + 1];
    bool vin_valid;
    
    char calibration_id[MAX_ECUS][CALIBRATION_ID_LENGTH + 1];
    u8 calibration_id_count;
    
    u8 cvn[MAX_ECUS][CVN_LENGTH];
    u8 cvn_count;
    
    char ecu_name[MAX_ECUS][ECU_NAME_LENGTH + 1];
    u8 ecu_name_count;
    
    u32 timestamp_ms;
} VehicleInfo_t;

typedef void (*VehicleInfoCallback_t)(VehicleInfoType_t type, const VehicleInfo_t* info, void* context);

typedef struct {
    ErrorHandler_t* error_handler;
    VehicleInfoCallback_t callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
} VehicleInfoManagerConfig_t;

typedef struct {
    VehicleInfo_t info;
    bool initialized;
    ErrorHandler_t* error_handler;
    VehicleInfoCallback_t callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
    u8 vin_buffer[20];
    u8 vin_buffer_idx;
} VehicleInfoManager_t;

Result_t VehicleInfoManager_Init(VehicleInfoManager_t* vim, const VehicleInfoManagerConfig_t* config);

Result_t VehicleInfoManager_ProcessResponse(VehicleInfoManager_t* vim, 
                                            VehicleInfoType_t type,
                                            const u8* data, 
                                            u16 length);

Result_t VehicleInfoManager_GetInfo(const VehicleInfoManager_t* vim, VehicleInfo_t* info);

Result_t VehicleInfoManager_GetVin(const VehicleInfoManager_t* vim, char* vin, u8 max_len);

bool VehicleInfoManager_HasVin(const VehicleInfoManager_t* vim);

Result_t VehicleInfoManager_Clear(VehicleInfoManager_t* vim);

const char* VehicleInfoManager_GetTypeString(VehicleInfoType_t type);

#endif
