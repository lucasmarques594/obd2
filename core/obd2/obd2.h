#ifndef OBD2_H
#define OBD2_H

#include "../types.h"
#include "../elm327/elm327.h"
#include "../error/error_handler.h"

#define OBD2_MAX_DATA_BYTES 7
#define OBD2_VIN_LENGTH 17
#define OBD2_ECU_NAME_LENGTH 20

typedef enum {
    OBD2_MODE_01_LIVE_DATA = 0x01,
    OBD2_MODE_02_FREEZE_FRAME = 0x02,
    OBD2_MODE_03_READ_DTCS = 0x03,
    OBD2_MODE_04_CLEAR_DTCS = 0x04,
    OBD2_MODE_05_O2_MONITORING = 0x05,
    OBD2_MODE_06_TEST_RESULTS = 0x06,
    OBD2_MODE_07_PENDING_DTCS = 0x07,
    OBD2_MODE_08_CONTROL_OPERATION = 0x08,
    OBD2_MODE_09_VEHICLE_INFO = 0x09,
    OBD2_MODE_0A_PERMANENT_DTCS = 0x0A,
    OBD2_MODE_MAX
} Obd2Mode_t;

typedef struct {
    u8 mode;
    u8 pid;
    u8 data[OBD2_MAX_DATA_BYTES];
    u8 data_length;
    bool valid;
} Obd2Frame_t;

typedef struct {
    u8 mode;
    u8 pid;
    u8 frame_number;
} Obd2Request_t;

typedef void (*Obd2ResponseCallback_t)(const Obd2Frame_t* frame, void* context);

typedef struct {
    Elm327_t* elm;
    ErrorHandler_t* error_handler;
    Obd2ResponseCallback_t response_callback;
    void* callback_context;
} Obd2Config_t;

typedef struct {
    Elm327_t* elm;
    ErrorHandler_t* error_handler;
    Obd2ResponseCallback_t response_callback;
    void* callback_context;
    Obd2Request_t pending_request;
    bool request_pending;
    bool initialized;
} Obd2_t;

Result_t Obd2_Init(Obd2_t* obd, const Obd2Config_t* config);

Result_t Obd2_SendRequest(Obd2_t* obd, u8 mode, u8 pid);

Result_t Obd2_SendRequestWithFrame(Obd2_t* obd, u8 mode, u8 pid, u8 frame);

Result_t Obd2_ProcessResponse(Obd2_t* obd, const u8* raw_data, u16 length, Obd2Frame_t* frame);

Result_t Obd2_ReadLiveData(Obd2_t* obd, u8 pid);

Result_t Obd2_ReadFreezeFrame(Obd2_t* obd, u8 pid, u8 frame);

Result_t Obd2_ReadDTCs(Obd2_t* obd);

Result_t Obd2_ReadPendingDTCs(Obd2_t* obd);

Result_t Obd2_ReadPermanentDTCs(Obd2_t* obd);

Result_t Obd2_ClearDTCs(Obd2_t* obd);

Result_t Obd2_ReadVehicleInfo(Obd2_t* obd, u8 info_type);

bool Obd2_IsBusy(const Obd2_t* obd);

const char* Obd2_GetModeString(Obd2Mode_t mode);

#endif
