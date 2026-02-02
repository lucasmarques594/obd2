#include "vehicle_info.h"
#include <string.h>

static const char* const type_strings[] = {
    [VEHICLE_INFO_VIN_COUNT] = "VIN Message Count",
    [VEHICLE_INFO_VIN] = "VIN",
    [VEHICLE_INFO_CAL_ID_COUNT] = "Calibration ID Count",
    [VEHICLE_INFO_CAL_ID] = "Calibration ID",
    [VEHICLE_INFO_CVN_COUNT] = "CVN Count",
    [VEHICLE_INFO_CVN] = "CVN",
    [VEHICLE_INFO_IPT_COUNT] = "In-use Performance Count",
    [VEHICLE_INFO_IPT] = "In-use Performance",
    [VEHICLE_INFO_ECU_NAME] = "ECU Name"
};

static void copy_string_safe(char* dest, const char* src, size_t max_len)
{
    if ((dest == NULL_PTR) || (max_len == 0U)) {
        return;
    }
    
    if (src == NULL_PTR) {
        dest[0] = '\0';
        return;
    }
    
    size_t i = 0U;
    while ((i < (max_len - 1U)) && (src[i] != '\0')) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static bool is_printable(u8 c)
{
    return ((c >= 0x20U) && (c <= 0x7EU));
}

Result_t VehicleInfoManager_Init(VehicleInfoManager_t* vim, const VehicleInfoManagerConfig_t* config)
{
    if (vim == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    vim->info.vin[0] = '\0';
    vim->info.vin_valid = false;
    vim->info.calibration_id_count = 0U;
    vim->info.cvn_count = 0U;
    vim->info.ecu_name_count = 0U;
    vim->info.timestamp_ms = 0U;
    
    for (u8 i = 0U; i < MAX_ECUS; i++) {
        vim->info.calibration_id[i][0] = '\0';
        vim->info.ecu_name[i][0] = '\0';
        for (u8 j = 0U; j < CVN_LENGTH; j++) {
            vim->info.cvn[i][j] = 0U;
        }
    }
    
    vim->vin_buffer_idx = 0U;
    
    vim->error_handler = config->error_handler;
    vim->callback = config->callback;
    vim->callback_context = config->callback_context;
    vim->get_timestamp_ms = config->get_timestamp_ms;
    vim->initialized = true;
    
    return RESULT_OK;
}

Result_t VehicleInfoManager_ProcessResponse(VehicleInfoManager_t* vim, 
                                            VehicleInfoType_t type,
                                            const u8* data, 
                                            u16 length)
{
    if (vim == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (data == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (vim->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (vim->get_timestamp_ms != NULL_PTR) {
        vim->info.timestamp_ms = vim->get_timestamp_ms();
    }
    
    switch (type) {
        case VEHICLE_INFO_VIN: {
            u8 start_idx = 0U;
            
            if ((length > 0U) && (data[0] < 0x20U)) {
                start_idx = 1U;
            }
            
            u8 vin_idx = 0U;
            
            for (u16 i = start_idx; (i < length) && (vin_idx < VIN_LENGTH); i++) {
                if (is_printable(data[i]) == true) {
                    vim->info.vin[vin_idx] = (char)data[i];
                    vin_idx++;
                }
            }
            
            vim->info.vin[vin_idx] = '\0';
            
            if (vin_idx == VIN_LENGTH) {
                vim->info.vin_valid = true;
            }
            break;
        }
        
        case VEHICLE_INFO_CAL_ID: {
            if (vim->info.calibration_id_count < MAX_ECUS) {
                u8 idx = vim->info.calibration_id_count;
                u8 char_idx = 0U;
                
                for (u16 i = 0U; (i < length) && (char_idx < CALIBRATION_ID_LENGTH); i++) {
                    if (is_printable(data[i]) == true) {
                        vim->info.calibration_id[idx][char_idx] = (char)data[i];
                        char_idx++;
                    }
                }
                
                vim->info.calibration_id[idx][char_idx] = '\0';
                vim->info.calibration_id_count++;
            }
            break;
        }
        
        case VEHICLE_INFO_CVN: {
            if (vim->info.cvn_count < MAX_ECUS) {
                u8 idx = vim->info.cvn_count;
                
                for (u8 i = 0U; (i < length) && (i < CVN_LENGTH); i++) {
                    vim->info.cvn[idx][i] = data[i];
                }
                
                vim->info.cvn_count++;
            }
            break;
        }
        
        case VEHICLE_INFO_ECU_NAME: {
            if (vim->info.ecu_name_count < MAX_ECUS) {
                u8 idx = vim->info.ecu_name_count;
                u8 char_idx = 0U;
                
                for (u16 i = 0U; (i < length) && (char_idx < ECU_NAME_LENGTH); i++) {
                    if (is_printable(data[i]) == true) {
                        vim->info.ecu_name[idx][char_idx] = (char)data[i];
                        char_idx++;
                    }
                }
                
                vim->info.ecu_name[idx][char_idx] = '\0';
                vim->info.ecu_name_count++;
            }
            break;
        }
        
        case VEHICLE_INFO_VIN_COUNT:
        case VEHICLE_INFO_CAL_ID_COUNT:
        case VEHICLE_INFO_CVN_COUNT:
        case VEHICLE_INFO_IPT_COUNT:
        case VEHICLE_INFO_IPT:
        case VEHICLE_INFO_MAX:
        default:
            break;
    }
    
    if (vim->callback != NULL_PTR) {
        vim->callback(type, &vim->info, vim->callback_context);
    }
    
    return RESULT_OK;
}

Result_t VehicleInfoManager_GetInfo(const VehicleInfoManager_t* vim, VehicleInfo_t* info)
{
    if (vim == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (info == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (vim->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    *info = vim->info;
    
    return RESULT_OK;
}

Result_t VehicleInfoManager_GetVin(const VehicleInfoManager_t* vim, char* vin, u8 max_len)
{
    if (vim == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (vin == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (vim->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (vim->info.vin_valid == false) {
        vin[0] = '\0';
        return RESULT_NO_DATA;
    }
    
    copy_string_safe(vin, vim->info.vin, max_len);
    
    return RESULT_OK;
}

bool VehicleInfoManager_HasVin(const VehicleInfoManager_t* vim)
{
    if (vim == NULL_PTR) {
        return false;
    }
    
    if (vim->initialized == false) {
        return false;
    }
    
    return vim->info.vin_valid;
}

Result_t VehicleInfoManager_Clear(VehicleInfoManager_t* vim)
{
    if (vim == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (vim->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    vim->info.vin[0] = '\0';
    vim->info.vin_valid = false;
    vim->info.calibration_id_count = 0U;
    vim->info.cvn_count = 0U;
    vim->info.ecu_name_count = 0U;
    
    vim->vin_buffer_idx = 0U;
    
    return RESULT_OK;
}

const char* VehicleInfoManager_GetTypeString(VehicleInfoType_t type)
{
    if (type >= VEHICLE_INFO_MAX) {
        return "Unknown";
    }
    
    if (type_strings[type] == NULL_PTR) {
        return "Unknown";
    }
    
    return type_strings[type];
}
