#ifndef ELM327_H
#define ELM327_H

#include "../types.h"
#include "../ring_buffer/ring_buffer.h"
#include "../error/error_handler.h"
#include "../logger/logger.h"

#define ELM_RX_BUFFER_SIZE 256
#define ELM_TX_BUFFER_SIZE 64
#define ELM_CMD_TIMEOUT_MS 5000
#define ELM_INIT_TIMEOUT_MS 10000

typedef enum {
    ELM_PROTOCOL_AUTO = 0,
    ELM_PROTOCOL_SAE_J1850_PWM = 1,
    ELM_PROTOCOL_SAE_J1850_VPW = 2,
    ELM_PROTOCOL_ISO_9141_2 = 3,
    ELM_PROTOCOL_ISO_14230_4_KWP_5BAUD = 4,
    ELM_PROTOCOL_ISO_14230_4_KWP_FAST = 5,
    ELM_PROTOCOL_ISO_15765_4_CAN_11BIT_500K = 6,
    ELM_PROTOCOL_ISO_15765_4_CAN_29BIT_500K = 7,
    ELM_PROTOCOL_ISO_15765_4_CAN_11BIT_250K = 8,
    ELM_PROTOCOL_ISO_15765_4_CAN_29BIT_250K = 9,
    ELM_PROTOCOL_SAE_J1939_CAN = 10,
    ELM_PROTOCOL_USER1_CAN = 11,
    ELM_PROTOCOL_USER2_CAN = 12,
    ELM_PROTOCOL_MAX
} ElmProtocol_t;

typedef enum {
    ELM_STATE_IDLE = 0,
    ELM_STATE_SENDING = 1,
    ELM_STATE_WAITING_RESPONSE = 2,
    ELM_STATE_PROCESSING = 3,
    ELM_STATE_ERROR = 4,
    ELM_STATE_MAX
} ElmState_t;

typedef enum {
    ELM_RESP_OK = 0,
    ELM_RESP_NO_DATA = 1,
    ELM_RESP_STOPPED = 2,
    ELM_RESP_ERROR = 3,
    ELM_RESP_UNABLE_TO_CONNECT = 4,
    ELM_RESP_BUS_INIT_ERROR = 5,
    ELM_RESP_BUS_ERROR = 6,
    ELM_RESP_CAN_ERROR = 7,
    ELM_RESP_BUFFER_FULL = 8,
    ELM_RESP_UNKNOWN = 9,
    ELM_RESP_MAX
} ElmResponse_t;

typedef struct {
    char version[32];
    ElmProtocol_t detected_protocol;
    bool echo_off;
    bool linefeed_off;
    bool spaces_off;
    bool headers_off;
    bool adaptive_timing;
    u8 voltage_raw;
} ElmInfo_t;

typedef Result_t (*ElmWriteCallback_t)(const u8* data, u16 length, void* context);
typedef Result_t (*ElmReadCallback_t)(u8* data, u16 max_length, u16* actual_length, void* context);

typedef struct {
    ElmWriteCallback_t write_callback;
    ElmReadCallback_t read_callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
    ErrorHandler_t* error_handler;
    Logger_t* logger;
} ElmConfig_t;

typedef struct {
    ElmState_t state;
    ElmInfo_t info;
    RingBuffer_t rx_ring;
    u8 rx_storage[ELM_RX_BUFFER_SIZE];
    u8 tx_buffer[ELM_TX_BUFFER_SIZE];
    u16 tx_length;
    u32 cmd_start_time_ms;
    u32 timeout_ms;
    bool initialized;
    ElmWriteCallback_t write_callback;
    ElmReadCallback_t read_callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
    ErrorHandler_t* error_handler;
    Logger_t* logger;
} Elm327_t;

Result_t Elm327_Init(Elm327_t* elm, const ElmConfig_t* config);

Result_t Elm327_Reset(Elm327_t* elm);

Result_t Elm327_Initialize(Elm327_t* elm);

Result_t Elm327_SetProtocol(Elm327_t* elm, ElmProtocol_t protocol);

Result_t Elm327_AutoDetectProtocol(Elm327_t* elm);

Result_t Elm327_SendCommand(Elm327_t* elm, const char* command);

Result_t Elm327_SendRawData(Elm327_t* elm, const u8* data, u16 length);

Result_t Elm327_ProcessRx(Elm327_t* elm);

Result_t Elm327_GetResponse(Elm327_t* elm, u8* buffer, u16 max_length, u16* actual_length);

ElmResponse_t Elm327_ParseResponse(const u8* data, u16 length);

Result_t Elm327_Update(Elm327_t* elm);

ElmState_t Elm327_GetState(const Elm327_t* elm);

ElmProtocol_t Elm327_GetProtocol(const Elm327_t* elm);

const ElmInfo_t* Elm327_GetInfo(const Elm327_t* elm);

bool Elm327_IsReady(const Elm327_t* elm);

bool Elm327_IsBusy(const Elm327_t* elm);

const char* Elm327_GetProtocolString(ElmProtocol_t protocol);

const char* Elm327_GetResponseString(ElmResponse_t response);

#endif
