#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "../types.h"
#include "../error/error_handler.h"

typedef enum {
    STATE_DISCONNECTED = 0,
    STATE_CONNECTING = 1,
    STATE_ELM_INIT = 2,
    STATE_PROTOCOL_DETECT = 3,
    STATE_VEHICLE_HANDSHAKE = 4,
    STATE_IDLE = 5,
    STATE_READING_PIDS = 6,
    STATE_READING_DTCS = 7,
    STATE_CLEARING_DTCS = 8,
    STATE_READING_FREEZE_FRAME = 9,
    STATE_READING_VEHICLE_INFO = 10,
    STATE_ERROR = 11,
    STATE_RECOVERY = 12,
    STATE_MAX
} State_t;

typedef enum {
    EVENT_NONE = 0,
    EVENT_CONNECT_REQUEST = 1,
    EVENT_DISCONNECT_REQUEST = 2,
    EVENT_CONNECTED = 3,
    EVENT_DISCONNECTED = 4,
    EVENT_ELM_INIT_COMPLETE = 5,
    EVENT_ELM_INIT_FAILED = 6,
    EVENT_PROTOCOL_DETECTED = 7,
    EVENT_PROTOCOL_FAILED = 8,
    EVENT_HANDSHAKE_COMPLETE = 9,
    EVENT_HANDSHAKE_FAILED = 10,
    EVENT_READ_PIDS_REQUEST = 11,
    EVENT_READ_DTCS_REQUEST = 12,
    EVENT_CLEAR_DTCS_REQUEST = 13,
    EVENT_READ_FREEZE_FRAME_REQUEST = 14,
    EVENT_READ_VEHICLE_INFO_REQUEST = 15,
    EVENT_OPERATION_COMPLETE = 16,
    EVENT_OPERATION_FAILED = 17,
    EVENT_TIMEOUT = 18,
    EVENT_ERROR = 19,
    EVENT_RECOVERY_COMPLETE = 20,
    EVENT_RECOVERY_FAILED = 21,
    EVENT_MAX
} Event_t;

typedef struct {
    State_t from_state;
    Event_t event;
    State_t to_state;
} StateTransition_t;

typedef void (*StateEntryHandler_t)(void* context);
typedef void (*StateExitHandler_t)(void* context);
typedef void (*StateTransitionCallback_t)(State_t from, State_t to, Event_t event, void* context);

typedef struct {
    u32 timeout_ms;
    u8 max_retries;
    StateEntryHandler_t on_entry;
    StateExitHandler_t on_exit;
} StateConfig_t;

typedef struct {
    State_t current_state;
    State_t previous_state;
    u32 state_entry_time_ms;
    u8 retry_count;
    bool initialized;
    void* context;
    StateTransitionCallback_t transition_callback;
    u32 (*get_timestamp_ms)(void);
    const StateConfig_t* state_configs;
    ErrorHandler_t* error_handler;
} StateMachine_t;

typedef struct {
    void* context;
    StateTransitionCallback_t transition_callback;
    u32 (*get_timestamp_ms)(void);
    const StateConfig_t* state_configs;
    ErrorHandler_t* error_handler;
} StateMachineConfig_t;

Result_t StateMachine_Init(StateMachine_t* sm, const StateMachineConfig_t* config);

Result_t StateMachine_ProcessEvent(StateMachine_t* sm, Event_t event);

Result_t StateMachine_Update(StateMachine_t* sm);

State_t StateMachine_GetCurrentState(const StateMachine_t* sm);

State_t StateMachine_GetPreviousState(const StateMachine_t* sm);

bool StateMachine_IsInState(const StateMachine_t* sm, State_t state);

u32 StateMachine_GetTimeInState(const StateMachine_t* sm);

bool StateMachine_IsTimedOut(const StateMachine_t* sm);

Result_t StateMachine_Reset(StateMachine_t* sm);

const char* StateMachine_GetStateString(State_t state);

const char* StateMachine_GetEventString(Event_t event);

bool StateMachine_CanTransition(const StateMachine_t* sm, Event_t event);

#endif
