#import "ble_elm327.h"
#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <pthread.h>
#import <string.h>

/* ---------------------------------------------------------------------------
 * Internal ring buffer for received BLE data
 * ---------------------------------------------------------------------------*/

typedef struct {
    u8 data[BLE_RX_BUFFER_SIZE];
    u16 head;
    u16 tail;
    u16 count;
    pthread_mutex_t mutex;
} BleRxBuffer_t;

static void rx_buf_init(BleRxBuffer_t* b) {
    b->head = 0; b->tail = 0; b->count = 0;
    pthread_mutex_init(&b->mutex, NULL);
}

static void rx_buf_push(BleRxBuffer_t* b, const u8* data, u16 len) {
    pthread_mutex_lock(&b->mutex);
    for (u16 i = 0; i < len; i++) {
        if (b->count < BLE_RX_BUFFER_SIZE) {
            b->data[b->head] = data[i];
            b->head = (b->head + 1) % BLE_RX_BUFFER_SIZE;
            b->count++;
        }
    }
    pthread_mutex_unlock(&b->mutex);
}

static u16 rx_buf_pop(BleRxBuffer_t* b, u8* out, u16 max_len) {
    pthread_mutex_lock(&b->mutex);
    u16 popped = 0;
    while (popped < max_len && b->count > 0) {
        out[popped] = b->data[b->tail];
        b->tail = (b->tail + 1) % BLE_RX_BUFFER_SIZE;
        b->count--;
        popped++;
    }
    pthread_mutex_unlock(&b->mutex);
    return popped;
}

static u16 rx_buf_count(BleRxBuffer_t* b) {
    pthread_mutex_lock(&b->mutex);
    u16 c = b->count;
    pthread_mutex_unlock(&b->mutex);
    return c;
}

static bool rx_buf_contains(BleRxBuffer_t* b, u8 byte) {
    pthread_mutex_lock(&b->mutex);
    bool found = false;
    u16 idx = b->tail;
    for (u16 i = 0; i < b->count; i++) {
        if (b->data[idx] == byte) { found = true; break; }
        idx = (idx + 1) % BLE_RX_BUFFER_SIZE;
    }
    pthread_mutex_unlock(&b->mutex);
    return found;
}

static void rx_buf_clear(BleRxBuffer_t* b) {
    pthread_mutex_lock(&b->mutex);
    b->head = 0; b->tail = 0; b->count = 0;
    pthread_mutex_unlock(&b->mutex);
}

/* ---------------------------------------------------------------------------
 * Objective-C delegate that wraps CoreBluetooth
 * ---------------------------------------------------------------------------*/

@interface BleDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>

@property (nonatomic, strong) CBCentralManager* central;
@property (nonatomic, strong) CBPeripheral* peripheral;
@property (nonatomic, strong) CBCharacteristic* txChar;
@property (nonatomic, strong) CBCharacteristic* rxChar;

@property (nonatomic, assign) BleState_t state;
@property (nonatomic, assign) bool poweredOn;
@property (nonatomic, assign) bool servicesDiscovered;

@property (nonatomic, strong) NSString* serviceUUID;
@property (nonatomic, strong) NSString* rxCharUUID;
@property (nonatomic, strong) NSString* txCharUUID;
@property (nonatomic, strong) NSString* nameFilter;

@property (nonatomic, assign) BleRxBuffer_t* rxBuffer;
@property (nonatomic, copy) NSString* deviceName;

@end

@implementation BleDelegate

- (instancetype)initWithRxBuffer:(BleRxBuffer_t*)buffer {
    self = [super init];
    if (self) {
        _rxBuffer = buffer;
        _state = BLE_STATE_IDLE;
        _poweredOn = false;
        _servicesDiscovered = false;
        _deviceName = @"";
        _central = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
    }
    return self;
}

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
    _poweredOn = (central.state == CBManagerStatePoweredOn);
    if (!_poweredOn) {
        NSLog(@"[BLE] Bluetooth nao esta ligado (state=%ld)", (long)central.state);
    }
}

- (void)startScan {
    if (!_poweredOn) {
        NSLog(@"[BLE] Aguardando Bluetooth ligar...");
        return;
    }
    _state = BLE_STATE_SCANNING;

    NSLog(@"[BLE] Escaneando (sem filtro de service)...");
    [_central scanForPeripheralsWithServices:nil
                                     options:@{CBCentralManagerScanOptionAllowDuplicatesKey: @NO}];
}

- (void)centralManager:(CBCentralManager*)central
    didDiscoverPeripheral:(CBPeripheral*)peripheral
    advertisementData:(NSDictionary<NSString*,id>*)advertisementData
    RSSI:(NSNumber*)RSSI
{
    NSString* name = peripheral.name ?: @"";
    NSLog(@"[BLE] Encontrado: %@ RSSI=%@", name, RSSI);

    bool match = false;
    if (_nameFilter.length == 0) {
        match = true;
    } else {
        match = [name.lowercaseString containsString:_nameFilter.lowercaseString];
    }

    if (match) {
        NSLog(@"[BLE] Conectando em %@...", name);
        [_central stopScan];
        _peripheral = peripheral;
        _peripheral.delegate = self;
        _deviceName = name;
        _state = BLE_STATE_CONNECTING;
        [_central connectPeripheral:peripheral options:nil];
    }
}

- (void)centralManager:(CBCentralManager*)central
    didConnectPeripheral:(CBPeripheral*)peripheral
{
    NSLog(@"[BLE] Conectado a %@", peripheral.name);
    _state = BLE_STATE_CONNECTED;

    NSArray* services = nil;
    if (_serviceUUID.length > 0) {
        services = @[[CBUUID UUIDWithString:_serviceUUID]];
    }
    [peripheral discoverServices:services];
}

- (void)centralManager:(CBCentralManager*)central
    didFailToConnectPeripheral:(CBPeripheral*)peripheral
    error:(NSError*)error
{
    NSLog(@"[BLE] Falha ao conectar: %@", error.localizedDescription);
    _state = BLE_STATE_ERROR;
}

- (void)centralManager:(CBCentralManager*)central
    didDisconnectPeripheral:(CBPeripheral*)peripheral
    error:(NSError*)error
{
    NSLog(@"[BLE] Desconectado: %@", error ? error.localizedDescription : @"normal");
    _state = BLE_STATE_IDLE;
    _txChar = nil;
    _rxChar = nil;
    _servicesDiscovered = false;
}

- (void)peripheral:(CBPeripheral*)peripheral
    didDiscoverServices:(NSError*)error
{
    if (error) {
        NSLog(@"[BLE] Erro ao descobrir servicos: %@", error.localizedDescription);
        _state = BLE_STATE_ERROR;
        return;
    }

    for (CBService* svc in peripheral.services) {
        NSLog(@"[BLE] Servico: %@", svc.UUID.UUIDString);
        [peripheral discoverCharacteristics:nil forService:svc];
    }
}

- (void)peripheral:(CBPeripheral*)peripheral
    didDiscoverCharacteristicsForService:(CBService*)service
    error:(NSError*)error
{
    if (error) {
        NSLog(@"[BLE] Erro characteristics: %@", error.localizedDescription);
        return;
    }

    CBUUID* rxUUID = [CBUUID UUIDWithString:_rxCharUUID];
    CBUUID* txUUID = [CBUUID UUIDWithString:_txCharUUID];

    for (CBCharacteristic* ch in service.characteristics) {
        NSLog(@"[BLE]   Char: %@ props=%lu", ch.UUID.UUIDString, (unsigned long)ch.properties);

        if ([ch.UUID isEqual:rxUUID]) {
            _rxChar = ch;
            [peripheral setNotifyValue:YES forCharacteristic:ch];
            NSLog(@"[BLE]   -> RX (notify) registrado");
        }

        if ([ch.UUID isEqual:txUUID]) {
            _txChar = ch;
            NSLog(@"[BLE]   -> TX (write) encontrado");
        }
    }

    if (_rxChar && _txChar) {
        _servicesDiscovered = true;
        _state = BLE_STATE_READY;
        NSLog(@"[BLE] Pronto para comunicacao!");
    }
}

- (void)peripheral:(CBPeripheral*)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic*)characteristic
    error:(NSError*)error
{
    if (error) {
        return;
    }

    NSData* data = characteristic.value;
    if (data && data.length > 0) {
        rx_buf_push(_rxBuffer, (const u8*)data.bytes, (u16)data.length);
    }
}

- (bool)writeData:(const u8*)data length:(u16)length {
    if (!_txChar || !_peripheral) {
        return false;
    }

    NSData* nsData = [NSData dataWithBytes:data length:length];

    CBCharacteristicWriteType writeType =
        (_txChar.properties & CBCharacteristicPropertyWriteWithoutResponse)
            ? CBCharacteristicWriteWithoutResponse
            : CBCharacteristicWriteWithResponse;

    [_peripheral writeValue:nsData forCharacteristic:_txChar type:writeType];
    return true;
}

- (void)disconnect {
    if (_peripheral) {
        [_central cancelPeripheralConnection:_peripheral];
    }
    _state = BLE_STATE_IDLE;
}

@end

/* ---------------------------------------------------------------------------
 * C-facing struct wrapping the ObjC delegate
 * ---------------------------------------------------------------------------*/

struct BleElm327 {
    BleConfig_t config;
    BleRxBuffer_t rx_buffer;
    BleState_t state;
    bool initialized;
    char device_name[BLE_DEVICE_NAME_MAX];
    void* delegate; /* BleDelegate* stored as void* for C compat */
};

static const char* const state_strings[] = {
    [BLE_STATE_IDLE] = "Idle",
    [BLE_STATE_SCANNING] = "Scanning",
    [BLE_STATE_CONNECTING] = "Connecting",
    [BLE_STATE_CONNECTED] = "Connected",
    [BLE_STATE_READY] = "Ready",
    [BLE_STATE_ERROR] = "Error"
};

/* ---------------------------------------------------------------------------
 * Public C API
 * ---------------------------------------------------------------------------*/

BleElm327_t* BleElm327_Create(void) {
    BleElm327_t* ble = (BleElm327_t*)calloc(1, sizeof(BleElm327_t));
    return ble;
}

void BleElm327_Destroy(BleElm327_t* ble) {
    if (!ble) return;
    if (ble->delegate) {
        BleDelegate* d = (__bridge_transfer BleDelegate*)ble->delegate;
        [d disconnect];
        d = nil;
    }
    free(ble);
}

Result_t BleElm327_Init(BleElm327_t* ble, const BleConfig_t* config) {
    if (!ble || !config) return RESULT_INVALID_PARAM;

    ble->config = *config;
    rx_buf_init(&ble->rx_buffer);

    @autoreleasepool {
        BleDelegate* d = [[BleDelegate alloc] initWithRxBuffer:&ble->rx_buffer];
        d.serviceUUID = [NSString stringWithUTF8String:config->service_uuid];
        d.rxCharUUID = [NSString stringWithUTF8String:config->rx_char_uuid];
        d.txCharUUID = [NSString stringWithUTF8String:config->tx_char_uuid];
        d.nameFilter = [NSString stringWithUTF8String:config->device_name_filter];
        ble->delegate = (__bridge_retained void*)d;
    }

    ble->initialized = true;
    ble->state = BLE_STATE_IDLE;
    return RESULT_OK;
}

Result_t BleElm327_InitDefault(BleElm327_t* ble) {
    if (!ble) return RESULT_INVALID_PARAM;

    BleConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.service_uuid, "0000FFF0-0000-1000-8000-00805F9B34FB");
    strcpy(cfg.rx_char_uuid, "0000FFF1-0000-1000-8000-00805F9B34FB");
    strcpy(cfg.tx_char_uuid, "0000FFF2-0000-1000-8000-00805F9B34FB");
    strcpy(cfg.device_name_filter, "OBD");
    cfg.scan_timeout_sec = BLE_DEFAULT_SCAN_TIMEOUT_SEC;
    cfg.connect_timeout_sec = BLE_DEFAULT_CONNECT_TIMEOUT_SEC;
    cfg.cmd_timeout_ms = BLE_DEFAULT_CMD_TIMEOUT_MS;

    return BleElm327_Init(ble, &cfg);
}

Result_t BleElm327_ScanAndConnect(BleElm327_t* ble) {
    if (!ble || !ble->initialized) return RESULT_INVALID_PARAM;

    BleDelegate* d = (__bridge BleDelegate*)ble->delegate;

    /* Wait for Bluetooth to power on */
    printf("  Aguardando Bluetooth...\n");
    int waited = 0;
    while (!d.poweredOn && waited < 5) {
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1.0]];
        waited++;
    }

    if (!d.poweredOn) {
        printf("  Bluetooth nao esta ligado.\n");
        ble->state = BLE_STATE_ERROR;
        return RESULT_ERROR;
    }

    printf("  Bluetooth ligado. Escaneando dispositivos BLE...\n");

    /* Start scanning */
    [d startScan];

    u32 timeout_ticks = (ble->config.scan_timeout_sec + ble->config.connect_timeout_sec) * 10U;
    u32 scan_timeout_ticks = ble->config.scan_timeout_sec * 10U;
    u32 elapsed = 0;

    while (d.state != BLE_STATE_READY && d.state != BLE_STATE_ERROR && elapsed < timeout_ticks) {
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        elapsed++;

        if (elapsed % 20 == 0) {
            printf("  ... %s (%u/%us)\n",
                   BleElm327_GetStateString(d.state),
                   elapsed / 10,
                   (ble->config.scan_timeout_sec + ble->config.connect_timeout_sec));
        }

        if (d.state == BLE_STATE_SCANNING && elapsed > scan_timeout_ticks) {
            [d.central stopScan];
            printf("  Timeout no scan — nenhum dispositivo encontrado.\n");
            ble->state = BLE_STATE_ERROR;
            return RESULT_TIMEOUT;
        }
    }

    ble->state = d.state;

    if (d.state == BLE_STATE_READY) {
        const char* name = d.deviceName.UTF8String ?: "desconhecido";
        strncpy(ble->device_name, name, BLE_DEVICE_NAME_MAX - 1);
        ble->device_name[BLE_DEVICE_NAME_MAX - 1] = '\0';
        return RESULT_OK;
    }

    return RESULT_ERROR;
}

Result_t BleElm327_Disconnect(BleElm327_t* ble) {
    if (!ble || !ble->initialized) return RESULT_INVALID_PARAM;

    BleDelegate* d = (__bridge BleDelegate*)ble->delegate;
    [d disconnect];
    ble->state = BLE_STATE_IDLE;
    return RESULT_OK;
}

Result_t BleElm327_Write(BleElm327_t* ble, const u8* data, u16 length) {
    if (!ble || !data) return RESULT_INVALID_PARAM;
    if (!ble->initialized) return RESULT_NOT_READY;

    BleDelegate* d = (__bridge BleDelegate*)ble->delegate;

    if (d.state != BLE_STATE_READY) return RESULT_NOT_READY;

    /* BLE has MTU limits — send in chunks of 20 bytes */
    u16 offset = 0;
    while (offset < length) {
        u16 chunk = (length - offset > 20) ? 20 : (length - offset);
        if (![d writeData:(data + offset) length:chunk]) {
            return RESULT_ERROR;
        }
        offset += chunk;
        if (offset < length) {
            usleep(10000); /* 10ms between chunks */
        }
    }

    return RESULT_OK;
}

Result_t BleElm327_Read(BleElm327_t* ble, u8* buffer, u16 max_length, u16* actual_length) {
    if (!ble || !buffer || !actual_length) return RESULT_INVALID_PARAM;
    if (!ble->initialized) return RESULT_NOT_READY;

    /* Pump the run loop to receive pending BLE notifications */
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];

    *actual_length = rx_buf_pop(&ble->rx_buffer, buffer, max_length);
    return RESULT_OK;
}

Result_t BleElm327_SendCommand(BleElm327_t* ble, const char* cmd,
                                char* response, u16 max_len, u16* resp_len,
                                u32 timeout_ms)
{
    if (!ble || !cmd || !response || !resp_len) return RESULT_INVALID_PARAM;

    rx_buf_clear(&ble->rx_buffer);

    /* Build command with CR */
    u16 cmd_len = (u16)strlen(cmd);
    u8 cmd_buf[128];
    if (cmd_len >= sizeof(cmd_buf) - 1) return RESULT_BUFFER_FULL;

    memcpy(cmd_buf, cmd, cmd_len);
    cmd_buf[cmd_len] = '\r';
    cmd_len++;

    Result_t r = BleElm327_Write(ble, cmd_buf, cmd_len);
    if (r != RESULT_OK) return r;

    printf("TX: %s\\r\n", cmd);

    /* Wait for '>' prompt */
    u16 total = 0;
    u32 elapsed = 0;

    while (elapsed < timeout_ms && total < (max_len - 1)) {
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];

        u8 tmp[128];
        u16 n = 0;
        BleElm327_Read(ble, tmp, sizeof(tmp), &n);

        for (u16 i = 0; i < n && total < (max_len - 1); i++) {
            if (tmp[i] != '>' && tmp[i] != '\r' && tmp[i] != '\n') {
                response[total++] = (char)tmp[i];
            }
            if (tmp[i] == '>') {
                goto done;
            }
        }

        elapsed += 10;
    }

done:
    response[total] = '\0';
    *resp_len = total;

    if (total > 0) {
        printf("RX: %s\n", response);
    }

    return (total > 0) ? RESULT_OK : RESULT_TIMEOUT;
}

BleState_t BleElm327_GetState(const BleElm327_t* ble) {
    if (!ble || !ble->initialized) return BLE_STATE_ERROR;

    BleDelegate* d = (__bridge BleDelegate*)ble->delegate;
    return d.state;
}

bool BleElm327_IsConnected(const BleElm327_t* ble) {
    if (!ble || !ble->initialized) return false;

    BleDelegate* d = (__bridge BleDelegate*)ble->delegate;
    return (d.state == BLE_STATE_READY);
}

u16 BleElm327_GetAvailableBytes(const BleElm327_t* ble) {
    if (!ble) return 0;
    return rx_buf_count((BleRxBuffer_t*)&ble->rx_buffer);
}

const char* BleElm327_GetDeviceName(const BleElm327_t* ble) {
    if (!ble) return "";
    return ble->device_name;
}

const char* BleElm327_GetStateString(BleState_t state) {
    if (state >= BLE_STATE_MAX) return "Unknown";
    return state_strings[state];
}

/* ---------------------------------------------------------------------------
 * Elm327 core callbacks — plug into Elm327_t
 * ---------------------------------------------------------------------------*/

Result_t BleElm327_WriteCallback(const u8* data, u16 length, void* context) {
    BleElm327_t* ble = (BleElm327_t*)context;
    if (!ble) return RESULT_INVALID_PARAM;

    printf("TX: %.*s", (int)length, (const char*)data);

    return BleElm327_Write(ble, data, length);
}

Result_t BleElm327_ReadCallback(u8* data, u16 max_length, u16* actual_length, void* context) {
    BleElm327_t* ble = (BleElm327_t*)context;
    if (!ble) return RESULT_INVALID_PARAM;

    return BleElm327_Read(ble, data, max_length, actual_length);
}
