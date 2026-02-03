#ifndef SANITY_CHECK_H
#define SANITY_CHECK_H

#include "../types.h"
#include "../pid/pid_manager.h"
#include "../error/error_handler.h"

#define SANITY_HISTORY_SIZE 8
#define SANITY_STUCK_THRESHOLD 5

typedef enum {
    SANITY_RESULT_OK = 0,
    SANITY_RESULT_OUT_OF_RANGE = 1,
    SANITY_RESULT_SENSOR_STUCK = 2,
    SANITY_RESULT_INVALID_DATA = 3,
    SANITY_RESULT_RATE_OF_CHANGE = 4,
    SANITY_RESULT_MAX
} SanityResult_t;

typedef struct {
    u8 pid;
    float min_value;
    float max_value;
    float max_rate_of_change;
    bool check_stuck;
    bool check_range;
    bool check_rate;
} SanityRule_t;

typedef struct {
    u8 pid;
    float history[SANITY_HISTORY_SIZE];
    u8 history_idx;
    u8 history_count;
    u8 stuck_count;
    u32 last_check_ms;
    bool valid;
} SanityHistory_t;

typedef void (*SanityFailCallback_t)(u8 pid, SanityResult_t result, float value, void* context);

typedef struct {
    ErrorHandler_t* error_handler;
    SanityFailCallback_t fail_callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
} SanityCheckConfig_t;

typedef struct {
    SanityHistory_t history[64];
    u8 history_count;
    bool initialized;
    ErrorHandler_t* error_handler;
    SanityFailCallback_t fail_callback;
    void* callback_context;
    u32 (*get_timestamp_ms)(void);
    u32 total_checks;
    u32 total_failures;
} SanityCheck_t;

Result_t SanityCheck_Init(SanityCheck_t* sc, const SanityCheckConfig_t* config);

SanityResult_t SanityCheck_ValidatePid(SanityCheck_t* sc, u8 pid, const PidValue_t* value);

SanityResult_t SanityCheck_ValidateRange(u8 pid, float value);

SanityResult_t SanityCheck_ValidateStuck(SanityCheck_t* sc, u8 pid, float value);

SanityResult_t SanityCheck_ValidateRateOfChange(SanityCheck_t* sc, u8 pid, float value);

Result_t SanityCheck_ClearHistory(SanityCheck_t* sc, u8 pid);

Result_t SanityCheck_ClearAllHistory(SanityCheck_t* sc);

const SanityRule_t* SanityCheck_GetRule(u8 pid);

bool SanityCheck_IsConfigured(u8 pid);

const char* SanityCheck_GetResultString(SanityResult_t result);

u32 SanityCheck_GetTotalChecks(const SanityCheck_t* sc);

u32 SanityCheck_GetTotalFailures(const SanityCheck_t* sc);

#endif
