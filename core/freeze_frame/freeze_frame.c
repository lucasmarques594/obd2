#include "freeze_frame.h"
#include <string.h>

static FreezeFrame_t* find_frame(FreezeFrameManager_t* ffm, u8 frame_number)
{
    for (u8 i = 0U; i < ffm->frame_count; i++) {
        if (ffm->frames[i].frame_number == frame_number) {
            return &ffm->frames[i];
        }
    }
    return NULL_PTR;
}

static const FreezeFrame_t* find_frame_const(const FreezeFrameManager_t* ffm, u8 frame_number)
{
    for (u8 i = 0U; i < ffm->frame_count; i++) {
        if (ffm->frames[i].frame_number == frame_number) {
            return &ffm->frames[i];
        }
    }
    return NULL_PTR;
}

static FreezeFrame_t* create_frame(FreezeFrameManager_t* ffm, u8 frame_number)
{
    if (ffm->frame_count >= FREEZE_FRAME_MAX_COUNT) {
        return NULL_PTR;
    }
    
    FreezeFrame_t* frame = &ffm->frames[ffm->frame_count];
    
    frame->frame_number = frame_number;
    frame->pid_count = 0U;
    frame->valid = false;
    frame->associated_dtc.valid = false;
    
    if (ffm->get_timestamp_ms != NULL_PTR) {
        frame->timestamp_ms = ffm->get_timestamp_ms();
    } else {
        frame->timestamp_ms = 0U;
    }
    
    ffm->frame_count++;
    
    return frame;
}

static FreezeFramePid_t* find_or_create_pid_entry(FreezeFrame_t* frame, u8 pid)
{
    for (u8 i = 0U; i < frame->pid_count; i++) {
        if (frame->pids[i].pid == pid) {
            return &frame->pids[i];
        }
    }
    
    if (frame->pid_count >= FREEZE_FRAME_MAX_PIDS) {
        return NULL_PTR;
    }
    
    FreezeFramePid_t* entry = &frame->pids[frame->pid_count];
    entry->pid = pid;
    entry->value.valid = false;
    frame->pid_count++;
    
    return entry;
}

Result_t FreezeFrameManager_Init(FreezeFrameManager_t* ffm, const FreezeFrameManagerConfig_t* config)
{
    if (ffm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    ffm->frame_count = 0U;
    ffm->current_frame_idx = 0U;
    ffm->current_pid_idx = 0U;
    
    for (u8 i = 0U; i < FREEZE_FRAME_MAX_COUNT; i++) {
        ffm->frames[i].valid = false;
        ffm->frames[i].pid_count = 0U;
    }
    
    ffm->error_handler = config->error_handler;
    ffm->callback = config->callback;
    ffm->callback_context = config->callback_context;
    ffm->get_timestamp_ms = config->get_timestamp_ms;
    ffm->initialized = true;
    
    return RESULT_OK;
}

Result_t FreezeFrameManager_ProcessResponse(FreezeFrameManager_t* ffm, const Obd2Frame_t* frame)
{
    if (ffm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (frame == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (ffm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (frame->valid == false) {
        return RESULT_INVALID_PARAM;
    }
    
    if (frame->mode != OBD2_MODE_02_FREEZE_FRAME) {
        return RESULT_INVALID_PARAM;
    }
    
    u8 frame_number = 0U;
    if (frame->data_length > 0U) {
        frame_number = frame->data[0];
    }
    
    FreezeFrame_t* ff = find_frame(ffm, frame_number);
    
    if (ff == NULL_PTR) {
        ff = create_frame(ffm, frame_number);
        
        if (ff == NULL_PTR) {
            if (ffm->error_handler != NULL_PTR) {
                ERROR_REPORT(ffm->error_handler, ERR_MEMORY_OVERFLOW, ERR_SEV_WARNING);
            }
            return RESULT_BUFFER_FULL;
        }
    }
    
    FreezeFramePid_t* pid_entry = find_or_create_pid_entry(ff, frame->pid);
    
    if (pid_entry == NULL_PTR) {
        return RESULT_BUFFER_FULL;
    }
    
    u8 data_offset = 1U;
    u8 data_len;
    
    if (frame->data_length > data_offset) {
        data_len = frame->data_length - data_offset;
    } else {
        data_len = 0U;
    }
    
    if (data_len > 0U) {
        Result_t result = PidManager_ConvertRawToEng(frame->pid, 
                                                     &frame->data[data_offset], 
                                                     data_len, 
                                                     &pid_entry->value);
        
        if (result == RESULT_OK) {
            if (ffm->get_timestamp_ms != NULL_PTR) {
                pid_entry->value.timestamp_ms = ffm->get_timestamp_ms();
            }
            
            ff->valid = true;
        }
    }
    
    return RESULT_OK;
}

Result_t FreezeFrameManager_SetDtc(FreezeFrameManager_t* ffm, u8 frame_number, const Dtc_t* dtc)
{
    if (ffm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (dtc == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (ffm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    FreezeFrame_t* ff = find_frame(ffm, frame_number);
    
    if (ff == NULL_PTR) {
        ff = create_frame(ffm, frame_number);
        
        if (ff == NULL_PTR) {
            return RESULT_BUFFER_FULL;
        }
    }
    
    ff->associated_dtc = *dtc;
    
    return RESULT_OK;
}

Result_t FreezeFrameManager_GetFrame(const FreezeFrameManager_t* ffm, u8 frame_number, FreezeFrame_t* frame)
{
    if (ffm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (frame == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (ffm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    const FreezeFrame_t* ff = find_frame_const(ffm, frame_number);
    
    if (ff == NULL_PTR) {
        frame->valid = false;
        return RESULT_NO_DATA;
    }
    
    *frame = *ff;
    
    return RESULT_OK;
}

u8 FreezeFrameManager_GetCount(const FreezeFrameManager_t* ffm)
{
    if (ffm == NULL_PTR) {
        return 0U;
    }
    
    if (ffm->initialized == false) {
        return 0U;
    }
    
    return ffm->frame_count;
}

Result_t FreezeFrameManager_Clear(FreezeFrameManager_t* ffm)
{
    if (ffm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (ffm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    for (u8 i = 0U; i < FREEZE_FRAME_MAX_COUNT; i++) {
        ffm->frames[i].valid = false;
        ffm->frames[i].pid_count = 0U;
        ffm->frames[i].associated_dtc.valid = false;
    }
    
    ffm->frame_count = 0U;
    ffm->current_frame_idx = 0U;
    ffm->current_pid_idx = 0U;
    
    return RESULT_OK;
}

Result_t FreezeFrameManager_ClearFrame(FreezeFrameManager_t* ffm, u8 frame_number)
{
    if (ffm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (ffm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    for (u8 i = 0U; i < ffm->frame_count; i++) {
        if (ffm->frames[i].frame_number == frame_number) {
            for (u8 j = i; j < (ffm->frame_count - 1U); j++) {
                ffm->frames[j] = ffm->frames[j + 1U];
            }
            
            ffm->frame_count--;
            ffm->frames[ffm->frame_count].valid = false;
            
            return RESULT_OK;
        }
    }
    
    return RESULT_NO_DATA;
}

bool FreezeFrameManager_HasFrame(const FreezeFrameManager_t* ffm, u8 frame_number)
{
    if (ffm == NULL_PTR) {
        return false;
    }
    
    if (ffm->initialized == false) {
        return false;
    }
    
    return (find_frame_const(ffm, frame_number) != NULL_PTR);
}

Result_t FreezeFrameManager_GetPidValue(const FreezeFrameManager_t* ffm, 
                                        u8 frame_number, 
                                        u8 pid, 
                                        PidValue_t* value)
{
    if (ffm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (value == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (ffm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    const FreezeFrame_t* ff = find_frame_const(ffm, frame_number);
    
    if (ff == NULL_PTR) {
        value->valid = false;
        return RESULT_NO_DATA;
    }
    
    for (u8 i = 0U; i < ff->pid_count; i++) {
        if (ff->pids[i].pid == pid) {
            *value = ff->pids[i].value;
            return RESULT_OK;
        }
    }
    
    value->valid = false;
    return RESULT_NO_DATA;
}
