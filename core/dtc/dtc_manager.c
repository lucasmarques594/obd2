#include "dtc_manager.h"
#include "../str_utils/str_utils.h"
#include <string.h>

static const char* const type_strings[] = {
    [DTC_TYPE_CURRENT] = "Current",
    [DTC_TYPE_PENDING] = "Pending",
    [DTC_TYPE_PERMANENT] = "Permanent",
    [DTC_TYPE_STORED] = "Stored"
};

static const char* const system_strings[] = {
    [DTC_SYSTEM_POWERTRAIN] = "Powertrain",
    [DTC_SYSTEM_CHASSIS] = "Chassis",
    [DTC_SYSTEM_BODY] = "Body",
    [DTC_SYSTEM_NETWORK] = "Network"
};

static const char system_chars[] = {'P', 'C', 'B', 'U'};


Result_t DtcManager_CodeToString(u16 raw_code, char* buffer, u8 buffer_len)
{
    if (buffer == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (buffer_len < DTC_CODE_STRING_LEN) {
        return RESULT_INVALID_PARAM;
    }
    
    u8 system_bits = (u8)((raw_code >> 14U) & 0x03U);
    u8 sub_system = (u8)((raw_code >> 12U) & 0x03U);
    u8 fault_high = (u8)((raw_code >> 8U) & 0x0FU);
    u8 fault_low = (u8)(raw_code & 0xFFU);
    
    if (system_bits < 4U) {
        buffer[0] = system_chars[system_bits];
    } else {
        buffer[0] = 'P';
    }
    
    buffer[1] = (char)('0' + sub_system);
    buffer[2] = nibble_to_hex_char(fault_high);
    buffer[3] = nibble_to_hex_char((fault_low >> 4U) & 0x0FU);
    buffer[4] = nibble_to_hex_char(fault_low & 0x0FU);
    buffer[5] = '\0';
    
    return RESULT_OK;
}

Result_t DtcManager_ParseDtc(u16 raw_code, Dtc_t* dtc)
{
    if (dtc == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (raw_code == 0x0000U) {
        dtc->valid = false;
        return RESULT_NO_DATA;
    }
    
    dtc->raw_code = raw_code;
    
    u8 system_bits = (u8)((raw_code >> 14U) & 0x03U);
    
    switch (system_bits) {
        case 0U:
            dtc->system = DTC_SYSTEM_POWERTRAIN;
            break;
        case 1U:
            dtc->system = DTC_SYSTEM_CHASSIS;
            break;
        case 2U:
            dtc->system = DTC_SYSTEM_BODY;
            break;
        case 3U:
            dtc->system = DTC_SYSTEM_NETWORK;
            break;
        default:
            dtc->system = DTC_SYSTEM_POWERTRAIN;
            break;
    }
    
    Result_t result = DtcManager_CodeToString(raw_code, dtc->code_string, DTC_CODE_STRING_LEN);
    
    if (result != RESULT_OK) {
        dtc->valid = false;
        return result;
    }
    
    dtc->valid = true;
    
    return RESULT_OK;
}

Result_t DtcManager_Init(DtcManager_t* dm, const DtcManagerConfig_t* config)
{
    if (dm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    dm->storage.current_count = 0U;
    dm->storage.pending_count = 0U;
    dm->storage.permanent_count = 0U;
    
    for (u8 i = 0U; i < DTC_MAX_COUNT; i++) {
        dm->storage.current[i].valid = false;
        dm->storage.pending[i].valid = false;
        dm->storage.permanent[i].valid = false;
    }
    
    dm->mil_status = false;
    dm->dtc_count_ecu = 0U;
    dm->error_handler = config->error_handler;
    dm->dtc_callback = config->dtc_callback;
    dm->cleared_callback = config->cleared_callback;
    dm->callback_context = config->callback_context;
    dm->get_timestamp_ms = config->get_timestamp_ms;
    dm->initialized = true;
    
    return RESULT_OK;
}

Result_t DtcManager_ProcessResponse(DtcManager_t* dm, const u8* data, u16 length, DtcType_t type)
{
    if (dm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (data == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (dm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (type >= DTC_TYPE_MAX) {
        return RESULT_INVALID_PARAM;
    }
    
    Dtc_t* storage = NULL_PTR;
    u8* count_ptr = NULL_PTR;
    
    switch (type) {
        case DTC_TYPE_CURRENT:
        case DTC_TYPE_STORED:
            storage = dm->storage.current;
            count_ptr = &dm->storage.current_count;
            break;
        case DTC_TYPE_PENDING:
            storage = dm->storage.pending;
            count_ptr = &dm->storage.pending_count;
            break;
        case DTC_TYPE_PERMANENT:
            storage = dm->storage.permanent;
            count_ptr = &dm->storage.permanent_count;
            break;
        default:
            return RESULT_INVALID_PARAM;
    }
    
    *count_ptr = 0U;
    
    u16 idx = 0U;
    
    while ((idx + 1U) < length) {
        if (*count_ptr >= DTC_MAX_COUNT) {
            break;
        }
        
        u16 raw_code = ((u16)data[idx] << 8U) | (u16)data[idx + 1U];
        idx += 2U;
        
        if (raw_code == 0x0000U) {
            continue;
        }
        
        Dtc_t* dtc = &storage[*count_ptr];
        
        Result_t result = DtcManager_ParseDtc(raw_code, dtc);
        
        if (result == RESULT_OK) {
            dtc->type = type;
            
            if (dm->get_timestamp_ms != NULL_PTR) {
                dtc->timestamp_ms = dm->get_timestamp_ms();
            } else {
                dtc->timestamp_ms = 0U;
            }
            
            (*count_ptr)++;
            
            if (dm->dtc_callback != NULL_PTR) {
                dm->dtc_callback(dtc, dm->callback_context);
            }
        }
    }
    
    return RESULT_OK;
}

Result_t DtcManager_Clear(DtcManager_t* dm)
{
    if (dm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (dm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    dm->storage.current_count = 0U;
    dm->storage.pending_count = 0U;
    
    for (u8 i = 0U; i < DTC_MAX_COUNT; i++) {
        dm->storage.current[i].valid = false;
        dm->storage.pending[i].valid = false;
    }
    
    dm->mil_status = false;
    dm->dtc_count_ecu = 0U;
    
    if (dm->cleared_callback != NULL_PTR) {
        dm->cleared_callback(dm->callback_context);
    }
    
    return RESULT_OK;
}

u8 DtcManager_GetCount(const DtcManager_t* dm, DtcType_t type)
{
    if (dm == NULL_PTR) {
        return 0U;
    }
    
    if (dm->initialized == false) {
        return 0U;
    }
    
    switch (type) {
        case DTC_TYPE_CURRENT:
        case DTC_TYPE_STORED:
            return dm->storage.current_count;
        case DTC_TYPE_PENDING:
            return dm->storage.pending_count;
        case DTC_TYPE_PERMANENT:
            return dm->storage.permanent_count;
        default:
            return 0U;
    }
}

u8 DtcManager_GetTotalCount(const DtcManager_t* dm)
{
    if (dm == NULL_PTR) {
        return 0U;
    }
    
    if (dm->initialized == false) {
        return 0U;
    }
    
    return dm->storage.current_count + dm->storage.pending_count + dm->storage.permanent_count;
}

Result_t DtcManager_GetDtc(const DtcManager_t* dm, DtcType_t type, u8 index, Dtc_t* dtc)
{
    if (dm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (dtc == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (dm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    const Dtc_t* storage = NULL_PTR;
    u8 count = 0U;
    
    switch (type) {
        case DTC_TYPE_CURRENT:
        case DTC_TYPE_STORED:
            storage = dm->storage.current;
            count = dm->storage.current_count;
            break;
        case DTC_TYPE_PENDING:
            storage = dm->storage.pending;
            count = dm->storage.pending_count;
            break;
        case DTC_TYPE_PERMANENT:
            storage = dm->storage.permanent;
            count = dm->storage.permanent_count;
            break;
        default:
            return RESULT_INVALID_PARAM;
    }
    
    if (index >= count) {
        return RESULT_INVALID_PARAM;
    }
    
    *dtc = storage[index];
    
    return RESULT_OK;
}

Result_t DtcManager_GetAllDtcs(const DtcManager_t* dm, DtcType_t type, Dtc_t* dtcs, u8 max_count, u8* actual_count)
{
    if (dm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (dtcs == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (actual_count == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (dm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    const Dtc_t* storage = NULL_PTR;
    u8 count = 0U;
    
    switch (type) {
        case DTC_TYPE_CURRENT:
        case DTC_TYPE_STORED:
            storage = dm->storage.current;
            count = dm->storage.current_count;
            break;
        case DTC_TYPE_PENDING:
            storage = dm->storage.pending;
            count = dm->storage.pending_count;
            break;
        case DTC_TYPE_PERMANENT:
            storage = dm->storage.permanent;
            count = dm->storage.permanent_count;
            break;
        default:
            *actual_count = 0U;
            return RESULT_INVALID_PARAM;
    }
    
    u8 copy_count = (count < max_count) ? count : max_count;
    
    for (u8 i = 0U; i < copy_count; i++) {
        dtcs[i] = storage[i];
    }
    
    *actual_count = copy_count;
    
    return RESULT_OK;
}

bool DtcManager_GetMilStatus(const DtcManager_t* dm)
{
    if (dm == NULL_PTR) {
        return false;
    }
    
    if (dm->initialized == false) {
        return false;
    }
    
    return dm->mil_status;
}

Result_t DtcManager_SetMilStatus(DtcManager_t* dm, bool status, u8 dtc_count)
{
    if (dm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (dm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    dm->mil_status = status;
    dm->dtc_count_ecu = dtc_count;
    
    return RESULT_OK;
}

const char* DtcManager_GetTypeString(DtcType_t type)
{
    if (type >= DTC_TYPE_MAX) {
        return "Unknown";
    }
    
    return type_strings[type];
}

const char* DtcManager_GetSystemString(DtcSystem_t system)
{
    if (system >= DTC_SYSTEM_MAX) {
        return "Unknown";
    }
    
    return system_strings[system];
}
