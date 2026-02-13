#include "elm327.h"
#include <string.h>

static const char* const protocol_strings[] = {
    [ELM_PROTOCOL_AUTO] = "AUTO",
    [ELM_PROTOCOL_SAE_J1850_PWM] = "SAE J1850 PWM",
    [ELM_PROTOCOL_SAE_J1850_VPW] = "SAE J1850 VPW",
    [ELM_PROTOCOL_ISO_9141_2] = "ISO 9141-2",
    [ELM_PROTOCOL_ISO_14230_4_KWP_5BAUD] = "ISO 14230-4 KWP 5 Baud",
    [ELM_PROTOCOL_ISO_14230_4_KWP_FAST] = "ISO 14230-4 KWP Fast",
    [ELM_PROTOCOL_ISO_15765_4_CAN_11BIT_500K] = "ISO 15765-4 CAN 11bit 500K",
    [ELM_PROTOCOL_ISO_15765_4_CAN_29BIT_500K] = "ISO 15765-4 CAN 29bit 500K",
    [ELM_PROTOCOL_ISO_15765_4_CAN_11BIT_250K] = "ISO 15765-4 CAN 11bit 250K",
    [ELM_PROTOCOL_ISO_15765_4_CAN_29BIT_250K] = "ISO 15765-4 CAN 29bit 250K",
    [ELM_PROTOCOL_SAE_J1939_CAN] = "SAE J1939 CAN",
    [ELM_PROTOCOL_USER1_CAN] = "User1 CAN",
    [ELM_PROTOCOL_USER2_CAN] = "User2 CAN"
};

static const char* const response_strings[] = {
    [ELM_RESP_OK] = "OK",
    [ELM_RESP_NO_DATA] = "NO DATA",
    [ELM_RESP_STOPPED] = "STOPPED",
    [ELM_RESP_ERROR] = "ERROR",
    [ELM_RESP_UNABLE_TO_CONNECT] = "UNABLE TO CONNECT",
    [ELM_RESP_BUS_INIT_ERROR] = "BUS INIT ERROR",
    [ELM_RESP_BUS_ERROR] = "BUS ERROR",
    [ELM_RESP_CAN_ERROR] = "CAN ERROR",
    [ELM_RESP_BUFFER_FULL] = "BUFFER FULL",
    [ELM_RESP_UNKNOWN] = "UNKNOWN"
};

static void rx_buffer_init(ElmRxBuffer_t* buf)
{
    buf->head = 0U;
    buf->tail = 0U;
    buf->count = 0U;
}

static bool rx_buffer_push(ElmRxBuffer_t* buf, u8 byte)
{
    if (buf->count >= ELM_RX_BUFFER_SIZE) {
        return false;
    }
    
    buf->buffer[buf->head] = byte;
    buf->head = (buf->head + 1U) % ELM_RX_BUFFER_SIZE;
    buf->count++;
    
    return true;
}

static bool rx_buffer_pop(ElmRxBuffer_t* buf, u8* byte)
{
    if (buf->count == 0U) {
        return false;
    }
    
    *byte = buf->buffer[buf->tail];
    buf->tail = (buf->tail + 1U) % ELM_RX_BUFFER_SIZE;
    buf->count--;
    
    return true;
}

static bool rx_buffer_contains_prompt(const ElmRxBuffer_t* buf)
{
    if (buf->count == 0U) {
        return false;
    }
    
    u16 idx = buf->tail;
    for (u16 i = 0U; i < buf->count; i++) {
        if (buf->buffer[idx] == '>') {
            return true;
        }
        idx = (idx + 1U) % ELM_RX_BUFFER_SIZE;
    }
    
    return false;
}

static bool str_contains(const u8* data, u16 length, const char* pattern)
{
    if ((data == NULL_PTR) || (pattern == NULL_PTR) || (length == 0U)) {
        return false;
    }
    
    size_t pattern_len = 0U;
    while (pattern[pattern_len] != '\0') {
        pattern_len++;
    }
    
    if (pattern_len > length) {
        return false;
    }
    
    for (u16 i = 0U; i <= (length - pattern_len); i++) {
        bool match = true;
        for (size_t j = 0U; j < pattern_len; j++) {
            if (data[i + j] != (u8)pattern[j]) {
                match = false;
                break;
            }
        }
        if (match == true) {
            return true;
        }
    }
    
    return false;
}

Result_t Elm327_Init(Elm327_t* elm, const ElmConfig_t* config)
{
    if (elm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config->write_callback == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config->read_callback == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    elm->state = ELM_STATE_IDLE;
    elm->write_callback = config->write_callback;
    elm->read_callback = config->read_callback;
    elm->callback_context = config->callback_context;
    elm->get_timestamp_ms = config->get_timestamp_ms;
    elm->error_handler = config->error_handler;
    elm->logger = config->logger;
    elm->cmd_start_time_ms = 0U;
    elm->timeout_ms = ELM_CMD_TIMEOUT_MS;
    elm->tx_length = 0U;
    
    rx_buffer_init(&elm->rx_buffer);
    
    elm->info.version[0] = '\0';
    elm->info.detected_protocol = ELM_PROTOCOL_AUTO;
    elm->info.echo_off = false;
    elm->info.linefeed_off = false;
    elm->info.spaces_off = false;
    elm->info.headers_off = false;
    elm->info.adaptive_timing = false;
    elm->info.voltage_raw = 0U;
    
    elm->initialized = true;
    
    return RESULT_OK;
}

Result_t Elm327_Reset(Elm327_t* elm)
{
    if (elm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (elm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    elm->timeout_ms = ELM_INIT_TIMEOUT_MS;
    
    return Elm327_SendCommand(elm, "ATZ");
}

Result_t Elm327_Initialize(Elm327_t* elm)
{
    if (elm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (elm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    Result_t result;
    
    result = Elm327_Reset(elm);
    if (result != RESULT_OK) {
        return result;
    }
    
    return RESULT_OK;
}

Result_t Elm327_SetProtocol(Elm327_t* elm, ElmProtocol_t protocol)
{
    if (elm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (elm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (protocol >= ELM_PROTOCOL_MAX) {
        return RESULT_INVALID_PARAM;
    }
    
    char cmd[8];
    cmd[0] = 'A';
    cmd[1] = 'T';
    cmd[2] = 'S';
    cmd[3] = 'P';
    
    if (protocol < 10U) {
        cmd[4] = (char)('0' + protocol);
        cmd[5] = '\0';
    } else {
        cmd[4] = 'A' + (char)(protocol - 10U);
        cmd[5] = '\0';
    }
    
    return Elm327_SendCommand(elm, cmd);
}

Result_t Elm327_AutoDetectProtocol(Elm327_t* elm)
{
    if (elm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (elm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    return Elm327_SetProtocol(elm, ELM_PROTOCOL_AUTO);
}

Result_t Elm327_SendCommand(Elm327_t* elm, const char* command)
{
    if (elm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (command == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (elm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (elm->state == ELM_STATE_SENDING || elm->state == ELM_STATE_WAITING_RESPONSE) {
        return RESULT_BUSY;
    }
    
    u16 cmd_len = 0U;
    while ((command[cmd_len] != '\0') && (cmd_len < (ELM_TX_BUFFER_SIZE - 2U))) {
        elm->tx_buffer[cmd_len] = (u8)command[cmd_len];
        cmd_len++;
    }
    
    elm->tx_buffer[cmd_len] = '\r';
    cmd_len++;
    elm->tx_buffer[cmd_len] = '\0';
    elm->tx_length = cmd_len;
    
    rx_buffer_init(&elm->rx_buffer);
    
    Result_t result = elm->write_callback(elm->tx_buffer, cmd_len, elm->callback_context);
    
    if (result != RESULT_OK) {
        elm->state = ELM_STATE_ERROR;
        if (elm->error_handler != NULL_PTR) {
            ERROR_REPORT(elm->error_handler, ERR_COMM_WRITE_FAILED, ERR_SEV_ERROR);
        }
        return result;
    }
    
    elm->state = ELM_STATE_WAITING_RESPONSE;
    
    if (elm->get_timestamp_ms != NULL_PTR) {
        elm->cmd_start_time_ms = elm->get_timestamp_ms();
    }
    
    if (elm->logger != NULL_PTR) {
        LOG_DEBUG(elm->logger, LOG_CAT_ELM, command, "", (u8)elm->info.detected_protocol);
    }
    
    return RESULT_OK;
}

Result_t Elm327_SendRawData(Elm327_t* elm, const u8* data, u16 length)
{
    if (elm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (data == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (elm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (elm->state == ELM_STATE_SENDING || elm->state == ELM_STATE_WAITING_RESPONSE) {
        return RESULT_BUSY;
    }
    
    if (length > (ELM_TX_BUFFER_SIZE - 1U)) {
        return RESULT_BUFFER_FULL;
    }
    
    for (u16 i = 0U; i < length; i++) {
        elm->tx_buffer[i] = data[i];
    }
    elm->tx_buffer[length] = '\r';
    elm->tx_length = length + 1U;
    
    rx_buffer_init(&elm->rx_buffer);
    
    Result_t result = elm->write_callback(elm->tx_buffer, elm->tx_length, elm->callback_context);
    
    if (result != RESULT_OK) {
        elm->state = ELM_STATE_ERROR;
        return result;
    }
    
    elm->state = ELM_STATE_WAITING_RESPONSE;
    
    if (elm->get_timestamp_ms != NULL_PTR) {
        elm->cmd_start_time_ms = elm->get_timestamp_ms();
    }
    
    return RESULT_OK;
}

Result_t Elm327_ProcessRx(Elm327_t* elm)
{
    if (elm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (elm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    u8 temp_buffer[64];
    u16 bytes_read = 0U;
    
    Result_t result = elm->read_callback(temp_buffer, sizeof(temp_buffer), &bytes_read, elm->callback_context);
    
    if (result != RESULT_OK) {
        return result;
    }
    
    for (u16 i = 0U; i < bytes_read; i++) {
        if (rx_buffer_push(&elm->rx_buffer, temp_buffer[i]) == false) {
            if (elm->error_handler != NULL_PTR) {
                ERROR_REPORT(elm->error_handler, ERR_COMM_BUFFER_OVERFLOW, ERR_SEV_WARNING);
            }
            return RESULT_BUFFER_FULL;
        }
    }
    
    if (rx_buffer_contains_prompt(&elm->rx_buffer) == true) {
        elm->state = ELM_STATE_IDLE;
    }
    
    return RESULT_OK;
}

Result_t Elm327_GetResponse(Elm327_t* elm, u8* buffer, u16 max_length, u16* actual_length)
{
    if (elm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (buffer == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (actual_length == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (elm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    *actual_length = 0U;
    u16 idx = 0U;
    u8 byte;
    
    while ((idx < max_length) && (rx_buffer_pop(&elm->rx_buffer, &byte) == true)) {
        if ((byte != '>') && (byte != '\r') && (byte != '\n')) {
            buffer[idx] = byte;
            idx++;
        }
    }
    
    *actual_length = idx;
    
    if (idx > 0U) {
        buffer[idx] = '\0';
    }
    
    return RESULT_OK;
}

ElmResponse_t Elm327_ParseResponse(const u8* data, u16 length)
{
    if ((data == NULL_PTR) || (length == 0U)) {
        return ELM_RESP_UNKNOWN;
    }
    
    if (str_contains(data, length, "OK") == true) {
        return ELM_RESP_OK;
    }
    
    if (str_contains(data, length, "NO DATA") == true) {
        return ELM_RESP_NO_DATA;
    }
    
    if (str_contains(data, length, "STOPPED") == true) {
        return ELM_RESP_STOPPED;
    }
    
    if (str_contains(data, length, "UNABLE TO CONNECT") == true) {
        return ELM_RESP_UNABLE_TO_CONNECT;
    }
    
    if (str_contains(data, length, "BUS INIT") == true) {
        return ELM_RESP_BUS_INIT_ERROR;
    }
    
    if (str_contains(data, length, "BUS ERROR") == true) {
        return ELM_RESP_BUS_ERROR;
    }
    
    if (str_contains(data, length, "CAN ERROR") == true) {
        return ELM_RESP_CAN_ERROR;
    }
    
    if (str_contains(data, length, "BUFFER FULL") == true) {
        return ELM_RESP_BUFFER_FULL;
    }
    
    if (str_contains(data, length, "ERROR") == true) {
        return ELM_RESP_ERROR;
    }
    
    if (str_contains(data, length, "ELM") == true) {
        return ELM_RESP_OK;
    }
    
    bool has_hex = false;
    for (u16 i = 0U; i < length; i++) {
        u8 c = data[i];
        if (((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'F'))) {
            has_hex = true;
            break;
        }
    }
    
    if (has_hex == true) {
        return ELM_RESP_OK;
    }
    
    return ELM_RESP_UNKNOWN;
}

Result_t Elm327_Update(Elm327_t* elm)
{
    if (elm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (elm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (elm->state == ELM_STATE_WAITING_RESPONSE) {
        Result_t result = Elm327_ProcessRx(elm);
        UNUSED(result);
        
        if (elm->get_timestamp_ms != NULL_PTR) {
            u32 current_time = elm->get_timestamp_ms();
            u32 elapsed;
            
            if (current_time >= elm->cmd_start_time_ms) {
                elapsed = current_time - elm->cmd_start_time_ms;
            } else {
                elapsed = (0xFFFFFFFFU - elm->cmd_start_time_ms) + current_time + 1U;
            }
            
            if (elapsed >= elm->timeout_ms) {
                elm->state = ELM_STATE_ERROR;
                if (elm->error_handler != NULL_PTR) {
                    ERROR_REPORT(elm->error_handler, ERR_COMM_TIMEOUT, ERR_SEV_ERROR);
                }
                return RESULT_TIMEOUT;
            }
        }
    }
    
    return RESULT_OK;
}

ElmState_t Elm327_GetState(const Elm327_t* elm)
{
    if (elm == NULL_PTR) {
        return ELM_STATE_ERROR;
    }
    
    if (elm->initialized == false) {
        return ELM_STATE_ERROR;
    }
    
    return elm->state;
}

ElmProtocol_t Elm327_GetProtocol(const Elm327_t* elm)
{
    if (elm == NULL_PTR) {
        return ELM_PROTOCOL_AUTO;
    }
    
    if (elm->initialized == false) {
        return ELM_PROTOCOL_AUTO;
    }
    
    return elm->info.detected_protocol;
}

const ElmInfo_t* Elm327_GetInfo(const Elm327_t* elm)
{
    if (elm == NULL_PTR) {
        return NULL_PTR;
    }
    
    if (elm->initialized == false) {
        return NULL_PTR;
    }
    
    return &elm->info;
}

bool Elm327_IsReady(const Elm327_t* elm)
{
    if (elm == NULL_PTR) {
        return false;
    }
    
    if (elm->initialized == false) {
        return false;
    }
    
    return (elm->state == ELM_STATE_IDLE);
}

bool Elm327_IsBusy(const Elm327_t* elm)
{
    if (elm == NULL_PTR) {
        return false;
    }
    
    if (elm->initialized == false) {
        return false;
    }
    
    return (elm->state == ELM_STATE_SENDING || elm->state == ELM_STATE_WAITING_RESPONSE);
}

const char* Elm327_GetProtocolString(ElmProtocol_t protocol)
{
    if (protocol >= ELM_PROTOCOL_MAX) {
        return "UNKNOWN";
    }
    
    return protocol_strings[protocol];
}

const char* Elm327_GetResponseString(ElmResponse_t response)
{
    if (response >= ELM_RESP_MAX) {
        return "UNKNOWN";
    }
    
    return response_strings[response];
}
