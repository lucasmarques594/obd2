#ifndef BLUETOOTH_IF_H
#define BLUETOOTH_IF_H

#include "../core/types.h"
#include "../core/error/error_handler.h"

#define BT_RX_BUFFER_SIZE 512
#define BT_TX_BUFFER_SIZE 256
#define BT_DEVICE_NAME_MAX 64
#define BT_UUID_STRING_MAX 48

typedef enum {
    BT_STATE_DISABLED = 0,
    BT_STATE_DISCONNECTED = 1,
    BT_STATE_SCANNING = 2,
    BT_STATE_CONNECTING = 3,
    BT_STATE_CONNECTED = 4,
    BT_STATE_ERROR = 5,
    BT_STATE_MAX
} BluetoothState_t;

typedef enum {
    BT_EVENT_NONE = 0,
    BT_EVENT_ENABLED = 1,
    BT_EVENT_DISABLED = 2,
    BT_EVENT_DEVICE_FOUND = 3,
    BT_EVENT_CONNECTED = 4,
    BT_EVENT_DISCONNECTED = 5,
    BT_EVENT_DATA_RECEIVED = 6,
    BT_EVENT_WRITE_COMPLETE = 7,
    BT_EVENT_ERROR = 8,
    BT_EVENT_MAX
} BluetoothEvent_t;

typedef struct {
    char name[BT_DEVICE_NAME_MAX];
    char uuid[BT_UUID_STRING_MAX];
    i8 rssi;
    bool is_elm327;
    bool valid;
} BluetoothDevice_t;

typedef struct {
    u8 buffer[BT_RX_BUFFER_SIZE];
    u16 head;
    u16 tail;
    u16 count;
} BluetoothRxBuffer_t;

typedef void (*BluetoothEventCallback_t)(BluetoothEvent_t event, const void* data, void* context);

typedef struct {
    BluetoothEventCallback_t event_callback;
    void* callback_context;
    ErrorHandler_t* error_handler;
} BluetoothConfig_t;

typedef struct {
    BluetoothState_t state;
    BluetoothDevice_t connected_device;
    BluetoothRxBuffer_t rx_buffer;
    u8 tx_buffer[BT_TX_BUFFER_SIZE];
    u16 tx_pending;
    bool initialized;
    BluetoothEventCallback_t event_callback;
    void* callback_context;
    ErrorHandler_t* error_handler;
    void* platform_handle;
} BluetoothInterface_t;

Result_t Bluetooth_Init(BluetoothInterface_t* bt, const BluetoothConfig_t* config);

Result_t Bluetooth_Deinit(BluetoothInterface_t* bt);

Result_t Bluetooth_StartScan(BluetoothInterface_t* bt);

Result_t Bluetooth_StopScan(BluetoothInterface_t* bt);

Result_t Bluetooth_Connect(BluetoothInterface_t* bt, const BluetoothDevice_t* device);

Result_t Bluetooth_Disconnect(BluetoothInterface_t* bt);

Result_t Bluetooth_Write(BluetoothInterface_t* bt, const u8* data, u16 length);

Result_t Bluetooth_Read(BluetoothInterface_t* bt, u8* buffer, u16 max_length, u16* actual_length);

u16 Bluetooth_GetAvailableBytes(const BluetoothInterface_t* bt);

BluetoothState_t Bluetooth_GetState(const BluetoothInterface_t* bt);

bool Bluetooth_IsConnected(const BluetoothInterface_t* bt);

Result_t Bluetooth_GetConnectedDevice(const BluetoothInterface_t* bt, BluetoothDevice_t* device);

Result_t Bluetooth_OnDataReceived(BluetoothInterface_t* bt, const u8* data, u16 length);

Result_t Bluetooth_OnStateChanged(BluetoothInterface_t* bt, BluetoothState_t new_state);

Result_t Bluetooth_OnDeviceFound(BluetoothInterface_t* bt, const BluetoothDevice_t* device);

void Bluetooth_SetPlatformHandle(BluetoothInterface_t* bt, void* handle);

void* Bluetooth_GetPlatformHandle(const BluetoothInterface_t* bt);

const char* Bluetooth_GetStateString(BluetoothState_t state);

const char* Bluetooth_GetEventString(BluetoothEvent_t event);

#endif
