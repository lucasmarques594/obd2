#include "obd2.h"
#include "../str_utils/str_utils.h"
#include <string.h>

static const char* const mode_strings[] = {
    [OBD2_MODE_01_LIVE_DATA] = "Live Data",
    [OBD2_MODE_02_FREEZE_FRAME] = "Freeze Frame",
    [OBD2_MODE_03_READ_DTCS] = "Read DTCs",
    [OBD2_MODE_04_CLEAR_DTCS] = "Clear DTCs",
    [OBD2_MODE_05_O2_MONITORING] = "O2 Monitoring",
    [OBD2_MODE_06_TEST_RESULTS] = "Test Results",
    [OBD2_MODE_07_PENDING_DTCS] = "Pending DTCs",
    [OBD2_MODE_08_CONTROL_OPERATION] = "Control Operation",
    [OBD2_MODE_09_VEHICLE_INFO] = "Vehicle Info",
    [OBD2_MODE_0A_PERMANENT_DTCS] = "Permanent DTCs"
};


static u16 build_obd_command(u8 mode, u8 pid, bool include_pid, char* buffer, u16 max_len)
{
    if ((buffer == NULL_PTR) || (max_len < 5U)) {
        return 0U;
    }
    
    u16 idx = 0U;
    
    buffer[idx] = nibble_to_hex_char((mode >> 4U) & 0x0FU);
    idx++;
    buffer[idx] = nibble_to_hex_char(mode & 0x0FU);
    idx++;
    
    if (include_pid == true) {
        buffer[idx] = nibble_to_hex_char((pid >> 4U) & 0x0FU);
        idx++;
        buffer[idx] = nibble_to_hex_char(pid & 0x0FU);
        idx++;
    }
    
    buffer[idx] = '\0';
    
    return idx;
}

static Result_t parse_hex_response(const u8* raw_data, u16 length, u8* out_bytes, u16 max_bytes, u16* out_length)
{
    if ((raw_data == NULL_PTR) || (out_bytes == NULL_PTR) || (out_length == NULL_PTR)) {
        return RESULT_INVALID_PARAM;
    }
    
    *out_length = 0U;
    u16 byte_idx = 0U;
    u16 raw_idx = 0U;
    
    while ((raw_idx < length) && (byte_idx < max_bytes)) {
        while ((raw_idx < length) && 
               (raw_data[raw_idx] == ' ' || raw_data[raw_idx] == '\r' || raw_data[raw_idx] == '\n')) {
            raw_idx++;
        }
        
        if (raw_idx >= length) {
            break;
        }
        
        u8 high_nibble = hex_char_to_nibble((char)raw_data[raw_idx]);
        if (high_nibble == 0xFFU) {
            raw_idx++;
            continue;
        }
        raw_idx++;
        
        if (raw_idx >= length) {
            break;
        }
        
        u8 low_nibble = hex_char_to_nibble((char)raw_data[raw_idx]);
        if (low_nibble == 0xFFU) {
            continue;
        }
        raw_idx++;
        
        out_bytes[byte_idx] = (high_nibble << 4U) | low_nibble;
        byte_idx++;
    }
    
    *out_length = byte_idx;
    
    return RESULT_OK;
}

Result_t Obd2_Init(Obd2_t* obd, const Obd2Config_t* config)
{
    if (obd == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config->elm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    obd->elm = config->elm;
    obd->error_handler = config->error_handler;
    obd->response_callback = config->response_callback;
    obd->callback_context = config->callback_context;
    obd->request_pending = false;
    obd->pending_request.mode = 0U;
    obd->pending_request.pid = 0U;
    obd->pending_request.frame_number = 0U;
    obd->initialized = true;
    
    return RESULT_OK;
}

Result_t Obd2_SendRequest(Obd2_t* obd, u8 mode, u8 pid)
{
    return Obd2_SendRequestWithFrame(obd, mode, pid, 0U);
}

Result_t Obd2_SendRequestWithFrame(Obd2_t* obd, u8 mode, u8 pid, u8 frame)
{
    if (obd == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (obd->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (Elm327_IsBusy(obd->elm) == true) {
        return RESULT_BUSY;
    }
    
    char cmd_buffer[16];
    bool include_pid = true;
    
    if ((mode == OBD2_MODE_03_READ_DTCS) || 
        (mode == OBD2_MODE_04_CLEAR_DTCS) ||
        (mode == OBD2_MODE_07_PENDING_DTCS) ||
        (mode == OBD2_MODE_0A_PERMANENT_DTCS)) {
        include_pid = false;
    }
    
    u16 cmd_len = build_obd_command(mode, pid, include_pid, cmd_buffer, sizeof(cmd_buffer));
    
    if (cmd_len == 0U) {
        return RESULT_ERROR;
    }
    
    if (frame > 0U) {
        if ((cmd_len + 2U) < sizeof(cmd_buffer)) {
            cmd_buffer[cmd_len] = nibble_to_hex_char((frame >> 4U) & 0x0FU);
            cmd_len++;
            cmd_buffer[cmd_len] = nibble_to_hex_char(frame & 0x0FU);
            cmd_len++;
            cmd_buffer[cmd_len] = '\0';
        }
    }
    
    obd->pending_request.mode = mode;
    obd->pending_request.pid = pid;
    obd->pending_request.frame_number = frame;
    obd->request_pending = true;
    
    return Elm327_SendCommand(obd->elm, cmd_buffer);
}

Result_t Obd2_ProcessResponse(Obd2_t* obd, const u8* raw_data, u16 length, Obd2Frame_t* frame)
{
    if (obd == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (raw_data == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (frame == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (obd->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    frame->valid = false;
    
    ElmResponse_t elm_resp = Elm327_ParseResponse(raw_data, length);
    
    if (elm_resp == ELM_RESP_NO_DATA) {
        if (obd->error_handler != NULL_PTR) {
            ERROR_REPORT(obd->error_handler, ERR_ELM_NO_DATA, ERR_SEV_INFO);
        }
        return RESULT_NO_DATA;
    }
    
    if (elm_resp == ELM_RESP_ERROR || elm_resp == ELM_RESP_CAN_ERROR) {
        if (obd->error_handler != NULL_PTR) {
            ERROR_REPORT(obd->error_handler, ERR_OBD_FRAME_ERROR, ERR_SEV_ERROR);
        }
        return RESULT_ERROR;
    }
    
    u8 parsed_bytes[16];
    u16 parsed_length = 0U;
    
    Result_t result = parse_hex_response(raw_data, length, parsed_bytes, sizeof(parsed_bytes), &parsed_length);
    
    if (result != RESULT_OK) {
        return result;
    }
    
    if (parsed_length < 2U) {
        return RESULT_ERROR;
    }
    
    u8 response_mode = parsed_bytes[0];
    
    if (response_mode != (obd->pending_request.mode + 0x40U)) {
        if (obd->error_handler != NULL_PTR) {
            ERROR_REPORT(obd->error_handler, ERR_OBD_INVALID_MODE, ERR_SEV_WARNING);
        }
        return RESULT_ERROR;
    }
    
    frame->mode = obd->pending_request.mode;
    frame->pid = parsed_bytes[1];
    
    u16 data_start = 2U;
    u16 data_len = parsed_length - data_start;
    
    if (data_len > OBD2_MAX_DATA_BYTES) {
        data_len = OBD2_MAX_DATA_BYTES;
    }
    
    for (u16 i = 0U; i < data_len; i++) {
        frame->data[i] = parsed_bytes[data_start + i];
    }
    frame->data_length = (u8)data_len;
    frame->valid = true;
    
    obd->request_pending = false;
    
    if ((obd->response_callback != NULL_PTR) && (frame->valid == true)) {
        obd->response_callback(frame, obd->callback_context);
    }
    
    return RESULT_OK;
}

Result_t Obd2_ReadLiveData(Obd2_t* obd, u8 pid)
{
    return Obd2_SendRequest(obd, OBD2_MODE_01_LIVE_DATA, pid);
}

Result_t Obd2_ReadFreezeFrame(Obd2_t* obd, u8 pid, u8 frame)
{
    return Obd2_SendRequestWithFrame(obd, OBD2_MODE_02_FREEZE_FRAME, pid, frame);
}

Result_t Obd2_ReadDTCs(Obd2_t* obd)
{
    return Obd2_SendRequest(obd, OBD2_MODE_03_READ_DTCS, 0U);
}

Result_t Obd2_ReadPendingDTCs(Obd2_t* obd)
{
    return Obd2_SendRequest(obd, OBD2_MODE_07_PENDING_DTCS, 0U);
}

Result_t Obd2_ReadPermanentDTCs(Obd2_t* obd)
{
    return Obd2_SendRequest(obd, OBD2_MODE_0A_PERMANENT_DTCS, 0U);
}

Result_t Obd2_ClearDTCs(Obd2_t* obd)
{
    return Obd2_SendRequest(obd, OBD2_MODE_04_CLEAR_DTCS, 0U);
}

Result_t Obd2_ReadVehicleInfo(Obd2_t* obd, u8 info_type)
{
    return Obd2_SendRequest(obd, OBD2_MODE_09_VEHICLE_INFO, info_type);
}

bool Obd2_IsBusy(const Obd2_t* obd)
{
    if (obd == NULL_PTR) {
        return false;
    }
    
    if (obd->initialized == false) {
        return false;
    }
    
    if (obd->request_pending == true) {
        return true;
    }
    
    return Elm327_IsBusy(obd->elm);
}

const char* Obd2_GetModeString(Obd2Mode_t mode)
{
    if (mode >= OBD2_MODE_MAX) {
        return "UNKNOWN";
    }
    
    if (mode == 0U) {
        return "UNKNOWN";
    }
    
    return mode_strings[mode];
}
