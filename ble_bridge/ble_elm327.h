#ifndef BLE_ELM327_H
#define BLE_ELM327_H

#include "../core/types.h"

#define BLE_DEVICE_NAME_MAX 64
#define BLE_RX_BUFFER_SIZE 4096
#define BLE_DEFAULT_SCAN_TIMEOUT_SEC 10
#define BLE_DEFAULT_CONNECT_TIMEOUT_SEC 15
#define BLE_DEFAULT_CMD_TIMEOUT_MS 5000

typedef enum {
    BLE_STATE_IDLE = 0,
    BLE_STATE_SCANNING = 1,
    BLE_STATE_CONNECTING = 2,
    BLE_STATE_CONNECTED = 3,
    BLE_STATE_READY = 4,
    BLE_STATE_ERROR = 5,
    BLE_STATE_MAX
} BleState_t;

typedef struct {
    char service_uuid[48];
    char rx_char_uuid[48];
    char tx_char_uuid[48];
    char device_name_filter[BLE_DEVICE_NAME_MAX];
    u32 scan_timeout_sec;
    u32 connect_timeout_sec;
    u32 cmd_timeout_ms;
} BleConfig_t;

typedef struct BleElm327 BleElm327_t;

BleElm327_t* BleElm327_Create(void);

void BleElm327_Destroy(BleElm327_t* ble);

Result_t BleElm327_Init(BleElm327_t* ble, const BleConfig_t* config);

Result_t BleElm327_InitDefault(BleElm327_t* ble);

Result_t BleElm327_ScanAndConnect(BleElm327_t* ble);

Result_t BleElm327_Disconnect(BleElm327_t* ble);

Result_t BleElm327_Write(BleElm327_t* ble, const u8* data, u16 length);

Result_t BleElm327_Read(BleElm327_t* ble, u8* buffer, u16 max_length, u16* actual_length);

Result_t BleElm327_SendCommand(BleElm327_t* ble, const char* cmd, char* response, u16 max_len, u16* resp_len, u32 timeout_ms);

BleState_t BleElm327_GetState(const BleElm327_t* ble);

bool BleElm327_IsConnected(const BleElm327_t* ble);

u16 BleElm327_GetAvailableBytes(const BleElm327_t* ble);

const char* BleElm327_GetDeviceName(const BleElm327_t* ble);

const char* BleElm327_GetStateString(BleState_t state);

Result_t BleElm327_WriteCallback(const u8* data, u16 length, void* context);

Result_t BleElm327_ReadCallback(u8* data, u16 max_length, u16* actual_length, void* context);

#endif
