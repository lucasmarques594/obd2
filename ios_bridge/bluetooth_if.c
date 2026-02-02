#include "bluetooth_if.h"
#include <string.h>

static const char* const state_strings[] = {
    [BT_STATE_DISABLED] = "Disabled",
    [BT_STATE_DISCONNECTED] = "Disconnected",
    [BT_STATE_SCANNING] = "Scanning",
    [BT_STATE_CONNECTING] = "Connecting",
    [BT_STATE_CONNECTED] = "Connected",
    [BT_STATE_ERROR] = "Error"
};

static const char* const event_strings[] = {
    [BT_EVENT_NONE] = "None",
    [BT_EVENT_ENABLED] = "Enabled",
    [BT_EVENT_DISABLED] = "Disabled",
    [BT_EVENT_DEVICE_FOUND] = "Device Found",
    [BT_EVENT_CONNECTED] = "Connected",
    [BT_EVENT_DISCONNECTED] = "Disconnected",
    [BT_EVENT_DATA_RECEIVED] = "Data Received",
    [BT_EVENT_WRITE_COMPLETE] = "Write Complete",
    [BT_EVENT_ERROR] = "Error"
};

static void rx_buffer_init(BluetoothRxBuffer_t* buf)
{
    buf->head = 0U;
    buf->tail = 0U;
    buf->count = 0U;
}

static bool rx_buffer_push(BluetoothRxBuffer_t* buf, u8 byte)
{
    if (buf->count >= BT_RX_BUFFER_SIZE) {
        return false;
    }
    
    buf->buffer[buf->head] = byte;
    buf->head = (buf->head + 1U) % BT_RX_BUFFER_SIZE;
    buf->count++;
    
    return true;
}

static bool rx_buffer_pop(BluetoothRxBuffer_t* buf, u8* byte)
{
    if (buf->count == 0U) {
        return false;
    }
    
    *byte = buf->buffer[buf->tail];
    buf->tail = (buf->tail + 1U) % BT_RX_BUFFER_SIZE;
    buf->count--;
    
    return true;
}

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

Result_t Bluetooth_Init(BluetoothInterface_t* bt, const BluetoothConfig_t* config)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    bt->state = BT_STATE_DISCONNECTED;
    bt->connected_device.valid = false;
    bt->connected_device.name[0] = '\0';
    bt->connected_device.uuid[0] = '\0';
    bt->tx_pending = 0U;
    bt->platform_handle = NULL_PTR;
    
    rx_buffer_init(&bt->rx_buffer);
    
    bt->event_callback = config->event_callback;
    bt->callback_context = config->callback_context;
    bt->error_handler = config->error_handler;
    bt->initialized = true;
    
    return RESULT_OK;
}

Result_t Bluetooth_Deinit(BluetoothInterface_t* bt)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (bt->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (bt->state == BT_STATE_CONNECTED) {
        Result_t result = Bluetooth_Disconnect(bt);
        UNUSED(result);
    }
    
    bt->initialized = false;
    bt->state = BT_STATE_DISABLED;
    
    return RESULT_OK;
}

Result_t Bluetooth_StartScan(BluetoothInterface_t* bt)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (bt->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (bt->state == BT_STATE_CONNECTED) {
        return RESULT_BUSY;
    }
    
    bt->state = BT_STATE_SCANNING;
    
    return RESULT_OK;
}

Result_t Bluetooth_StopScan(BluetoothInterface_t* bt)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (bt->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (bt->state == BT_STATE_SCANNING) {
        bt->state = BT_STATE_DISCONNECTED;
    }
    
    return RESULT_OK;
}

Result_t Bluetooth_Connect(BluetoothInterface_t* bt, const BluetoothDevice_t* device)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (device == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (bt->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (bt->state == BT_STATE_CONNECTED) {
        return RESULT_BUSY;
    }
    
    bt->state = BT_STATE_CONNECTING;
    
    copy_string_safe(bt->connected_device.name, device->name, BT_DEVICE_NAME_MAX);
    copy_string_safe(bt->connected_device.uuid, device->uuid, BT_UUID_STRING_MAX);
    bt->connected_device.rssi = device->rssi;
    bt->connected_device.is_elm327 = device->is_elm327;
    
    return RESULT_OK;
}

Result_t Bluetooth_Disconnect(BluetoothInterface_t* bt)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (bt->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    bt->state = BT_STATE_DISCONNECTED;
    bt->connected_device.valid = false;
    
    rx_buffer_init(&bt->rx_buffer);
    
    if (bt->event_callback != NULL_PTR) {
        bt->event_callback(BT_EVENT_DISCONNECTED, NULL_PTR, bt->callback_context);
    }
    
    return RESULT_OK;
}

Result_t Bluetooth_Write(BluetoothInterface_t* bt, const u8* data, u16 length)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (data == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (bt->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (bt->state != BT_STATE_CONNECTED) {
        return RESULT_NOT_READY;
    }
    
    if (length > BT_TX_BUFFER_SIZE) {
        return RESULT_BUFFER_FULL;
    }
    
    for (u16 i = 0U; i < length; i++) {
        bt->tx_buffer[i] = data[i];
    }
    bt->tx_pending = length;
    
    return RESULT_OK;
}

Result_t Bluetooth_Read(BluetoothInterface_t* bt, u8* buffer, u16 max_length, u16* actual_length)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (buffer == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (actual_length == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (bt->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    *actual_length = 0U;
    u16 idx = 0U;
    u8 byte;
    
    while ((idx < max_length) && (rx_buffer_pop(&bt->rx_buffer, &byte) == true)) {
        buffer[idx] = byte;
        idx++;
    }
    
    *actual_length = idx;
    
    return RESULT_OK;
}

u16 Bluetooth_GetAvailableBytes(const BluetoothInterface_t* bt)
{
    if (bt == NULL_PTR) {
        return 0U;
    }
    
    if (bt->initialized == false) {
        return 0U;
    }
    
    return bt->rx_buffer.count;
}

BluetoothState_t Bluetooth_GetState(const BluetoothInterface_t* bt)
{
    if (bt == NULL_PTR) {
        return BT_STATE_ERROR;
    }
    
    if (bt->initialized == false) {
        return BT_STATE_DISABLED;
    }
    
    return bt->state;
}

bool Bluetooth_IsConnected(const BluetoothInterface_t* bt)
{
    if (bt == NULL_PTR) {
        return false;
    }
    
    if (bt->initialized == false) {
        return false;
    }
    
    return (bt->state == BT_STATE_CONNECTED);
}

Result_t Bluetooth_GetConnectedDevice(const BluetoothInterface_t* bt, BluetoothDevice_t* device)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (device == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (bt->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (bt->state != BT_STATE_CONNECTED) {
        device->valid = false;
        return RESULT_NO_DATA;
    }
    
    *device = bt->connected_device;
    
    return RESULT_OK;
}

Result_t Bluetooth_OnDataReceived(BluetoothInterface_t* bt, const u8* data, u16 length)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (data == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (bt->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    for (u16 i = 0U; i < length; i++) {
        if (rx_buffer_push(&bt->rx_buffer, data[i]) == false) {
            if (bt->error_handler != NULL_PTR) {
                ERROR_REPORT(bt->error_handler, ERR_COMM_BUFFER_OVERFLOW, ERR_SEV_WARNING);
            }
            return RESULT_BUFFER_FULL;
        }
    }
    
    if (bt->event_callback != NULL_PTR) {
        bt->event_callback(BT_EVENT_DATA_RECEIVED, &length, bt->callback_context);
    }
    
    return RESULT_OK;
}

Result_t Bluetooth_OnStateChanged(BluetoothInterface_t* bt, BluetoothState_t new_state)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (bt->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (new_state >= BT_STATE_MAX) {
        return RESULT_INVALID_PARAM;
    }
    
    BluetoothState_t old_state = bt->state;
    bt->state = new_state;
    
    if (new_state == BT_STATE_CONNECTED) {
        bt->connected_device.valid = true;
        
        if (bt->event_callback != NULL_PTR) {
            bt->event_callback(BT_EVENT_CONNECTED, &bt->connected_device, bt->callback_context);
        }
    } else if ((old_state == BT_STATE_CONNECTED) && (new_state != BT_STATE_CONNECTED)) {
        bt->connected_device.valid = false;
        rx_buffer_init(&bt->rx_buffer);
        
        if (bt->event_callback != NULL_PTR) {
            bt->event_callback(BT_EVENT_DISCONNECTED, NULL_PTR, bt->callback_context);
        }
    }
    
    return RESULT_OK;
}

Result_t Bluetooth_OnDeviceFound(BluetoothInterface_t* bt, const BluetoothDevice_t* device)
{
    if (bt == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (device == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (bt->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (bt->event_callback != NULL_PTR) {
        bt->event_callback(BT_EVENT_DEVICE_FOUND, device, bt->callback_context);
    }
    
    return RESULT_OK;
}

void Bluetooth_SetPlatformHandle(BluetoothInterface_t* bt, void* handle)
{
    if (bt != NULL_PTR) {
        bt->platform_handle = handle;
    }
}

void* Bluetooth_GetPlatformHandle(const BluetoothInterface_t* bt)
{
    if (bt == NULL_PTR) {
        return NULL_PTR;
    }
    
    return bt->platform_handle;
}

const char* Bluetooth_GetStateString(BluetoothState_t state)
{
    if (state >= BT_STATE_MAX) {
        return "Unknown";
    }
    
    return state_strings[state];
}

const char* Bluetooth_GetEventString(BluetoothEvent_t event)
{
    if (event >= BT_EVENT_MAX) {
        return "Unknown";
    }
    
    return event_strings[event];
}
