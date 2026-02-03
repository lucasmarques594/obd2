#include "pid_manager.h"
#include <string.h>

static const char* const unit_strings[] = {
    [PID_UNIT_NONE] = "",
    [PID_UNIT_PERCENT] = "%",
    [PID_UNIT_DEGREES_C] = "°C",
    [PID_UNIT_KPA] = "kPa",
    [PID_UNIT_RPM] = "RPM",
    [PID_UNIT_KMH] = "km/h",
    [PID_UNIT_DEGREES] = "°",
    [PID_UNIT_GRAMS_SEC] = "g/s",
    [PID_UNIT_SECONDS] = "s",
    [PID_UNIT_KM] = "km",
    [PID_UNIT_VOLTS] = "V",
    [PID_UNIT_MINUTES] = "min",
    [PID_UNIT_RATIO] = "",
    [PID_UNIT_COUNT] = "",
    [PID_UNIT_PA] = "Pa",
    [PID_UNIT_MA] = "mA",
    [PID_UNIT_NM] = "Nm",
    [PID_UNIT_LPH] = "L/h"
};

static const PidDefinition_t pid_definitions[] = {
    {0x00, "PIDs supported [01-20]", "PIDS_A", PID_UNIT_NONE, PID_DATA_BITFIELD, 4, 0, 0, 1, 0, PID_PRIORITY_HIGH, 0},
    {0x01, "Monitor status", "MIL_STATUS", PID_UNIT_NONE, PID_DATA_BITFIELD, 4, 0, 0, 1, 0, PID_PRIORITY_HIGH, 1000},
    {0x03, "Fuel system status", "FUEL_SYS", PID_UNIT_NONE, PID_DATA_BITFIELD, 2, 0, 0, 1, 0, PID_PRIORITY_LOW, 5000},
    {0x04, "Calculated engine load", "ENGINE_LOAD", PID_UNIT_PERCENT, PID_DATA_U8, 1, 0, 100, 0.392157f, 0, PID_PRIORITY_HIGH, 250},
    {0x05, "Engine coolant temp", "COOLANT_TEMP", PID_UNIT_DEGREES_C, PID_DATA_U8, 1, -40, 215, 1, -40, PID_PRIORITY_MEDIUM, 1000},
    {0x06, "Short term fuel trim Bank 1", "STFT_B1", PID_UNIT_PERCENT, PID_DATA_U8, 1, -100, 99.2f, 0.78125f, -100, PID_PRIORITY_MEDIUM, 500},
    {0x07, "Long term fuel trim Bank 1", "LTFT_B1", PID_UNIT_PERCENT, PID_DATA_U8, 1, -100, 99.2f, 0.78125f, -100, PID_PRIORITY_LOW, 2000},
    {0x08, "Short term fuel trim Bank 2", "STFT_B2", PID_UNIT_PERCENT, PID_DATA_U8, 1, -100, 99.2f, 0.78125f, -100, PID_PRIORITY_MEDIUM, 500},
    {0x09, "Long term fuel trim Bank 2", "LTFT_B2", PID_UNIT_PERCENT, PID_DATA_U8, 1, -100, 99.2f, 0.78125f, -100, PID_PRIORITY_LOW, 2000},
    {0x0A, "Fuel pressure", "FUEL_PRESS", PID_UNIT_KPA, PID_DATA_U8, 1, 0, 765, 3, 0, PID_PRIORITY_MEDIUM, 1000},
    {0x0B, "Intake manifold pressure", "MAP", PID_UNIT_KPA, PID_DATA_U8, 1, 0, 255, 1, 0, PID_PRIORITY_HIGH, 250},
    {0x0C, "Engine RPM", "RPM", PID_UNIT_RPM, PID_DATA_U16, 2, 0, 16383.75f, 0.25f, 0, PID_PRIORITY_HIGH, 100},
    {0x0D, "Vehicle speed", "SPEED", PID_UNIT_KMH, PID_DATA_U8, 1, 0, 255, 1, 0, PID_PRIORITY_HIGH, 250},
    {0x0E, "Timing advance", "TIMING_ADV", PID_UNIT_DEGREES, PID_DATA_U8, 1, -64, 63.5f, 0.5f, -64, PID_PRIORITY_MEDIUM, 500},
    {0x0F, "Intake air temperature", "IAT", PID_UNIT_DEGREES_C, PID_DATA_U8, 1, -40, 215, 1, -40, PID_PRIORITY_MEDIUM, 1000},
    {0x10, "MAF air flow rate", "MAF", PID_UNIT_GRAMS_SEC, PID_DATA_U16, 2, 0, 655.35f, 0.01f, 0, PID_PRIORITY_HIGH, 250},
    {0x11, "Throttle position", "TPS", PID_UNIT_PERCENT, PID_DATA_U8, 1, 0, 100, 0.392157f, 0, PID_PRIORITY_HIGH, 100},
    {0x1C, "OBD standards", "OBD_STD", PID_UNIT_NONE, PID_DATA_U8, 1, 0, 255, 1, 0, PID_PRIORITY_LOW, 0},
    {0x1F, "Run time since engine start", "RUN_TIME", PID_UNIT_SECONDS, PID_DATA_U16, 2, 0, 65535, 1, 0, PID_PRIORITY_LOW, 5000},
    {0x20, "PIDs supported [21-40]", "PIDS_B", PID_UNIT_NONE, PID_DATA_BITFIELD, 4, 0, 0, 1, 0, PID_PRIORITY_HIGH, 0},
    {0x21, "Distance with MIL on", "MIL_DIST", PID_UNIT_KM, PID_DATA_U16, 2, 0, 65535, 1, 0, PID_PRIORITY_LOW, 5000},
    {0x2F, "Fuel tank level", "FUEL_LEVEL", PID_UNIT_PERCENT, PID_DATA_U8, 1, 0, 100, 0.392157f, 0, PID_PRIORITY_LOW, 5000},
    {0x31, "Distance since codes cleared", "CLR_DIST", PID_UNIT_KM, PID_DATA_U16, 2, 0, 65535, 1, 0, PID_PRIORITY_LOW, 5000},
    {0x33, "Barometric pressure", "BARO", PID_UNIT_KPA, PID_DATA_U8, 1, 0, 255, 1, 0, PID_PRIORITY_LOW, 10000},
    {0x40, "PIDs supported [41-60]", "PIDS_C", PID_UNIT_NONE, PID_DATA_BITFIELD, 4, 0, 0, 1, 0, PID_PRIORITY_HIGH, 0},
    {0x42, "Control module voltage", "CTRL_VOLT", PID_UNIT_VOLTS, PID_DATA_U16, 2, 0, 65.535f, 0.001f, 0, PID_PRIORITY_LOW, 5000},
    {0x43, "Absolute load value", "ABS_LOAD", PID_UNIT_PERCENT, PID_DATA_U16, 2, 0, 25700, 0.392157f, 0, PID_PRIORITY_MEDIUM, 500},
    {0x44, "Commanded AFR", "CMD_AFR", PID_UNIT_RATIO, PID_DATA_U16, 2, 0, 2, 0.0000305f, 0, PID_PRIORITY_MEDIUM, 500},
    {0x45, "Relative throttle position", "REL_TPS", PID_UNIT_PERCENT, PID_DATA_U8, 1, 0, 100, 0.392157f, 0, PID_PRIORITY_HIGH, 100},
    {0x46, "Ambient air temperature", "AMB_TEMP", PID_UNIT_DEGREES_C, PID_DATA_U8, 1, -40, 215, 1, -40, PID_PRIORITY_LOW, 10000},
    {0x47, "Absolute throttle position B", "ABS_TPS_B", PID_UNIT_PERCENT, PID_DATA_U8, 1, 0, 100, 0.392157f, 0, PID_PRIORITY_MEDIUM, 250},
    {0x49, "Accelerator pedal position D", "ACCEL_D", PID_UNIT_PERCENT, PID_DATA_U8, 1, 0, 100, 0.392157f, 0, PID_PRIORITY_HIGH, 100},
    {0x4A, "Accelerator pedal position E", "ACCEL_E", PID_UNIT_PERCENT, PID_DATA_U8, 1, 0, 100, 0.392157f, 0, PID_PRIORITY_HIGH, 100},
    {0x4C, "Commanded throttle actuator", "CMD_THROT", PID_UNIT_PERCENT, PID_DATA_U8, 1, 0, 100, 0.392157f, 0, PID_PRIORITY_MEDIUM, 250},
    {0x4D, "Time run with MIL on", "MIL_TIME", PID_UNIT_MINUTES, PID_DATA_U16, 2, 0, 65535, 1, 0, PID_PRIORITY_LOW, 5000},
    {0x4E, "Time since codes cleared", "CLR_TIME", PID_UNIT_MINUTES, PID_DATA_U16, 2, 0, 65535, 1, 0, PID_PRIORITY_LOW, 5000},
    {0x51, "Fuel type", "FUEL_TYPE", PID_UNIT_NONE, PID_DATA_U8, 1, 0, 255, 1, 0, PID_PRIORITY_LOW, 0},
    {0x5C, "Engine oil temperature", "OIL_TEMP", PID_UNIT_DEGREES_C, PID_DATA_U8, 1, -40, 210, 1, -40, PID_PRIORITY_MEDIUM, 2000},
    {0x5E, "Engine fuel rate", "FUEL_RATE", PID_UNIT_LPH, PID_DATA_U16, 2, 0, 3276.75f, 0.05f, 0, PID_PRIORITY_MEDIUM, 1000},
    {0x60, "PIDs supported [61-80]", "PIDS_D", PID_UNIT_NONE, PID_DATA_BITFIELD, 4, 0, 0, 1, 0, PID_PRIORITY_HIGH, 0},
    {0x62, "Actual engine torque %", "ACT_TORQ", PID_UNIT_PERCENT, PID_DATA_U8, 1, -125, 130, 1, -125, PID_PRIORITY_MEDIUM, 500},
    {0x63, "Engine reference torque", "REF_TORQ", PID_UNIT_NM, PID_DATA_U16, 2, 0, 65535, 1, 0, PID_PRIORITY_LOW, 0},
    {0x80, "PIDs supported [81-A0]", "PIDS_E", PID_UNIT_NONE, PID_DATA_BITFIELD, 4, 0, 0, 1, 0, PID_PRIORITY_HIGH, 0},
    {0xA0, "PIDs supported [A1-C0]", "PIDS_F", PID_UNIT_NONE, PID_DATA_BITFIELD, 4, 0, 0, 1, 0, PID_PRIORITY_HIGH, 0},
    {0xC0, "PIDs supported [C1-E0]", "PIDS_G", PID_UNIT_NONE, PID_DATA_BITFIELD, 4, 0, 0, 1, 0, PID_PRIORITY_HIGH, 0},
    {0xFF, NULL, NULL, PID_UNIT_NONE, PID_DATA_U8, 0, 0, 0, 0, 0, PID_PRIORITY_MAX, 0}
};

#define PID_DEFINITIONS_COUNT (sizeof(pid_definitions) / sizeof(pid_definitions[0]) - 1)

static const PidDefinition_t* find_pid_definition(u8 pid)
{
    for (u32 i = 0U; i < PID_DEFINITIONS_COUNT; i++) {
        if (pid_definitions[i].pid == pid) {
            return &pid_definitions[i];
        }
    }
    return NULL_PTR;
}

static PidEntry_t* find_or_create_entry(PidManager_t* pm, u8 pid)
{
    for (u8 i = 0U; i < pm->entry_count; i++) {
        if (pm->entries[i].pid == pid) {
            return &pm->entries[i];
        }
    }
    
    if (pm->entry_count < 64U) {
        PidEntry_t* entry = &pm->entries[pm->entry_count];
        entry->pid = pid;
        entry->supported = false;
        entry->enabled = false;
        entry->rate_ms = 1000U;
        entry->last_read_ms = 0U;
        entry->value.valid = false;
        pm->entry_count++;
        return entry;
    }
    
    return NULL_PTR;
}

static const PidEntry_t* find_entry(const PidManager_t* pm, u8 pid)
{
    for (u8 i = 0U; i < pm->entry_count; i++) {
        if (pm->entries[i].pid == pid) {
            return &pm->entries[i];
        }
    }
    return NULL_PTR;
}

Result_t PidManager_Init(PidManager_t* pm, const PidManagerConfig_t* config)
{
    if (pm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    for (u8 i = 0U; i < 32U; i++) {
        pm->supported_pids[i] = 0U;
    }
    
    pm->entry_count = 0U;
    pm->error_handler = config->error_handler;
    pm->value_callback = config->value_callback;
    pm->callback_context = config->callback_context;
    pm->get_timestamp_ms = config->get_timestamp_ms;
    pm->initialized = true;
    
    return RESULT_OK;
}

Result_t PidManager_SetSupported(PidManager_t* pm, const u8* supported_data, u8 start_pid)
{
    if (pm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (supported_data == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (pm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    for (u8 byte_idx = 0U; byte_idx < 4U; byte_idx++) {
        for (u8 bit_idx = 0U; bit_idx < 8U; bit_idx++) {
            u8 pid = start_pid + (byte_idx * 8U) + bit_idx + 1U;
            bool supported = ((supported_data[byte_idx] >> (7U - bit_idx)) & 0x01U) != 0U;
            
            u8 byte_pos = pid / 8U;
            u8 bit_pos = pid % 8U;
            
            if (byte_pos < 32U) {
                if (supported) {
                    pm->supported_pids[byte_pos] |= (1U << bit_pos);
                } else {
                    pm->supported_pids[byte_pos] &= ~(1U << bit_pos);
                }
                
                if (supported) {
                    PidEntry_t* entry = find_or_create_entry(pm, pid);
                    if (entry != NULL_PTR) {
                        entry->supported = true;
                        
                        const PidDefinition_t* def = find_pid_definition(pid);
                        if (def != NULL_PTR) {
                            entry->rate_ms = def->default_rate_ms;
                        }
                    }
                }
            }
        }
    }
    
    return RESULT_OK;
}

bool PidManager_IsSupported(const PidManager_t* pm, u8 pid)
{
    if (pm == NULL_PTR) {
        return false;
    }
    
    if (pm->initialized == false) {
        return false;
    }
    
    u8 byte_pos = pid / 8U;
    u8 bit_pos = pid % 8U;
    
    if (byte_pos >= 32U) {
        return false;
    }
    
    return ((pm->supported_pids[byte_pos] >> bit_pos) & 0x01U) != 0U;
}

Result_t PidManager_EnablePid(PidManager_t* pm, u8 pid, u16 rate_ms)
{
    if (pm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (pm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    PidEntry_t* entry = find_or_create_entry(pm, pid);
    
    if (entry == NULL_PTR) {
        return RESULT_BUFFER_FULL;
    }
    
    entry->enabled = true;
    entry->rate_ms = rate_ms;
    
    return RESULT_OK;
}

Result_t PidManager_DisablePid(PidManager_t* pm, u8 pid)
{
    if (pm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (pm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    for (u8 i = 0U; i < pm->entry_count; i++) {
        if (pm->entries[i].pid == pid) {
            pm->entries[i].enabled = false;
            return RESULT_OK;
        }
    }
    
    return RESULT_OK;
}

Result_t PidManager_SetRate(PidManager_t* pm, u8 pid, u16 rate_ms)
{
    if (pm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (pm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    for (u8 i = 0U; i < pm->entry_count; i++) {
        if (pm->entries[i].pid == pid) {
            pm->entries[i].rate_ms = rate_ms;
            return RESULT_OK;
        }
    }
    
    return RESULT_ERROR;
}

Result_t PidManager_ProcessFrame(PidManager_t* pm, const Obd2Frame_t* frame)
{
    if (pm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (frame == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (pm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (frame->valid == false) {
        return RESULT_INVALID_PARAM;
    }
    
    if (frame->mode != OBD2_MODE_01_LIVE_DATA) {
        return RESULT_OK;
    }
    
    PidEntry_t* entry = find_or_create_entry(pm, frame->pid);
    
    if (entry == NULL_PTR) {
        return RESULT_BUFFER_FULL;
    }
    
    PidValue_t value;
    Result_t result = PidManager_ConvertRawToEng(frame->pid, frame->data, frame->data_length, &value);
    
    if (result == RESULT_OK) {
        if (pm->get_timestamp_ms != NULL_PTR) {
            value.timestamp_ms = pm->get_timestamp_ms();
            entry->last_read_ms = value.timestamp_ms;
        }
        
        entry->value = value;
        
        if (pm->value_callback != NULL_PTR) {
            pm->value_callback(frame->pid, &value, pm->callback_context);
        }
    }
    
    return result;
}

Result_t PidManager_GetValue(const PidManager_t* pm, u8 pid, PidValue_t* value)
{
    if (pm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (value == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (pm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    const PidEntry_t* entry = find_entry(pm, pid);
    
    if (entry == NULL_PTR) {
        value->valid = false;
        return RESULT_NO_DATA;
    }
    
    *value = entry->value;
    
    return RESULT_OK;
}

Result_t PidManager_GetNextPidToRead(const PidManager_t* pm, u8* pid)
{
    if (pm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (pid == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (pm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    u32 current_time = 0U;
    if (pm->get_timestamp_ms != NULL_PTR) {
        current_time = pm->get_timestamp_ms();
    }
    
    u8 best_pid = 0xFFU;
    u8 best_priority = PID_PRIORITY_MAX;
    u32 best_overdue = 0U;
    
    for (u8 i = 0U; i < pm->entry_count; i++) {
        const PidEntry_t* entry = &pm->entries[i];
        
        if ((entry->enabled == false) || (entry->rate_ms == 0U)) {
            continue;
        }
        
        u32 elapsed;
        if (current_time >= entry->last_read_ms) {
            elapsed = current_time - entry->last_read_ms;
        } else {
            elapsed = (0xFFFFFFFFU - entry->last_read_ms) + current_time + 1U;
        }
        
        if (elapsed >= entry->rate_ms) {
            const PidDefinition_t* def = find_pid_definition(entry->pid);
            u8 priority = (def != NULL_PTR) ? def->priority : PID_PRIORITY_LOW;
            
            u32 overdue = elapsed - entry->rate_ms;
            
            if ((priority < best_priority) || 
                ((priority == best_priority) && (overdue > best_overdue))) {
                best_pid = entry->pid;
                best_priority = priority;
                best_overdue = overdue;
            }
        }
    }
    
    if (best_pid == 0xFFU) {
        return RESULT_NO_DATA;
    }
    
    *pid = best_pid;
    return RESULT_OK;
}

const PidDefinition_t* PidManager_GetDefinition(u8 pid)
{
    return find_pid_definition(pid);
}

Result_t PidManager_ConvertRawToEng(u8 pid, const u8* raw_data, u8 data_len, PidValue_t* value)
{
    if (raw_data == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (value == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    value->valid = false;
    
    const PidDefinition_t* def = find_pid_definition(pid);
    
    if (def == NULL_PTR) {
        if (data_len >= 1U) {
            value->raw_value = (i32)raw_data[0];
            value->eng_value = (float)raw_data[0];
            value->unit = PID_UNIT_NONE;
            value->valid = true;
        }
        return RESULT_OK;
    }
    
    if (data_len < def->data_bytes) {
        return RESULT_ERROR;
    }
    
    i32 raw = 0;
    
    switch (def->data_type) {
        case PID_DATA_U8:
            raw = (i32)raw_data[0];
            break;
            
        case PID_DATA_U16:
            raw = (i32)(((u16)raw_data[0] << 8U) | (u16)raw_data[1]);
            break;
            
        case PID_DATA_U32:
            raw = (i32)(((u32)raw_data[0] << 24U) | 
                        ((u32)raw_data[1] << 16U) |
                        ((u32)raw_data[2] << 8U) | 
                        (u32)raw_data[3]);
            break;
            
        case PID_DATA_I8:
            raw = (i32)(i8)raw_data[0];
            break;
            
        case PID_DATA_I16:
            raw = (i32)(i16)(((u16)raw_data[0] << 8U) | (u16)raw_data[1]);
            break;
            
        case PID_DATA_BITFIELD:
            raw = (i32)(((u32)raw_data[0] << 24U) | 
                        ((u32)raw_data[1] << 16U) |
                        ((u32)raw_data[2] << 8U) | 
                        (u32)raw_data[3]);
            break;
            
        case PID_DATA_FLOAT:
        case PID_DATA_MAX:
        default:
            raw = (i32)raw_data[0];
            break;
    }
    
    value->raw_value = raw;
    value->eng_value = ((float)raw * def->scale) + def->offset;
    value->unit = def->unit;
    value->valid = true;
    
    return RESULT_OK;
}

const char* PidManager_GetUnitString(PidUnit_t unit)
{
    if (unit >= PID_UNIT_MAX) {
        return "";
    }
    
    return unit_strings[unit];
}

u8 PidManager_GetSupportedCount(const PidManager_t* pm)
{
    if (pm == NULL_PTR) {
        return 0U;
    }
    
    if (pm->initialized == false) {
        return 0U;
    }
    
    u8 count = 0U;
    
    for (u8 i = 0U; i < pm->entry_count; i++) {
        if (pm->entries[i].supported == true) {
            count++;
        }
    }
    
    return count;
}

u8 PidManager_GetEnabledCount(const PidManager_t* pm)
{
    if (pm == NULL_PTR) {
        return 0U;
    }
    
    if (pm->initialized == false) {
        return 0U;
    }
    
    u8 count = 0U;
    
    for (u8 i = 0U; i < pm->entry_count; i++) {
        if (pm->entries[i].enabled == true) {
            count++;
        }
    }
    
    return count;
}
