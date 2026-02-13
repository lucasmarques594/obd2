#ifndef FREEZE_FRAME_H
#define FREEZE_FRAME_H

#include "../types.h"
#include "../obd2/obd2.h"
#include "../dtc/dtc_manager.h"
#include "../pid/pid_manager.h"
#include "../error/error_handler.h"

#define FREEZE_FRAME_MAX_COUNT 8
#define FREEZE_FRAME_MAX_PIDS 16

typedef struct {
    u8 pid;
    PidValue_t value;
} FreezeFramePid_t;

typedef struct {
    Dtc_t associated_dtc;
    u8 frame_number;
    FreezeFramePid_t pids[FREEZE_FRAME_MAX_PIDS];
    u8 pid_count;
    u32 timestamp_ms;
    bool valid;
} FreezeFrame_t;

typedef void (*FreezeFrameCallback_t)(const FreezeFrame_t* frame, void* context);

typedef struct {
    ErrorHandler_t* error_handler;
    FreezeFrameCallback_t callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
} FreezeFrameManagerConfig_t;

typedef struct {
    FreezeFrame_t frames[FREEZE_FRAME_MAX_COUNT];
    u8 frame_count;
    bool initialized;
    ErrorHandler_t* error_handler;
    FreezeFrameCallback_t callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
    u8 current_frame_idx;
    u8 current_pid_idx;
} FreezeFrameManager_t;

Result_t FreezeFrameManager_Init(FreezeFrameManager_t* ffm, const FreezeFrameManagerConfig_t* config);

Result_t FreezeFrameManager_ProcessResponse(FreezeFrameManager_t* ffm, const Obd2Frame_t* frame);

Result_t FreezeFrameManager_SetDtc(FreezeFrameManager_t* ffm, u8 frame_number, const Dtc_t* dtc);

Result_t FreezeFrameManager_GetFrame(const FreezeFrameManager_t* ffm, u8 frame_number, FreezeFrame_t* frame);

u8 FreezeFrameManager_GetCount(const FreezeFrameManager_t* ffm);

Result_t FreezeFrameManager_Clear(FreezeFrameManager_t* ffm);

Result_t FreezeFrameManager_ClearFrame(FreezeFrameManager_t* ffm, u8 frame_number);

bool FreezeFrameManager_HasFrame(const FreezeFrameManager_t* ffm, u8 frame_number);

Result_t FreezeFrameManager_GetPidValue(const FreezeFrameManager_t* ffm, 
                                        u8 frame_number, 
                                        u8 pid, 
                                        PidValue_t* value);

#endif
