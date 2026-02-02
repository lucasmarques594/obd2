#include "state_machine.h"

static const char* const state_strings[] = {
    [STATE_DISCONNECTED] = "DISCONNECTED",
    [STATE_CONNECTING] = "CONNECTING",
    [STATE_ELM_INIT] = "ELM_INIT",
    [STATE_PROTOCOL_DETECT] = "PROTOCOL_DETECT",
    [STATE_VEHICLE_HANDSHAKE] = "VEHICLE_HANDSHAKE",
    [STATE_IDLE] = "IDLE",
    [STATE_READING_PIDS] = "READING_PIDS",
    [STATE_READING_DTCS] = "READING_DTCS",
    [STATE_CLEARING_DTCS] = "CLEARING_DTCS",
    [STATE_READING_FREEZE_FRAME] = "READING_FREEZE_FRAME",
    [STATE_READING_VEHICLE_INFO] = "READING_VEHICLE_INFO",
    [STATE_ERROR] = "ERROR",
    [STATE_RECOVERY] = "RECOVERY"
};

static const char* const event_strings[] = {
    [EVENT_NONE] = "NONE",
    [EVENT_CONNECT_REQUEST] = "CONNECT_REQUEST",
    [EVENT_DISCONNECT_REQUEST] = "DISCONNECT_REQUEST",
    [EVENT_CONNECTED] = "CONNECTED",
    [EVENT_DISCONNECTED] = "DISCONNECTED",
    [EVENT_ELM_INIT_COMPLETE] = "ELM_INIT_COMPLETE",
    [EVENT_ELM_INIT_FAILED] = "ELM_INIT_FAILED",
    [EVENT_PROTOCOL_DETECTED] = "PROTOCOL_DETECTED",
    [EVENT_PROTOCOL_FAILED] = "PROTOCOL_FAILED",
    [EVENT_HANDSHAKE_COMPLETE] = "HANDSHAKE_COMPLETE",
    [EVENT_HANDSHAKE_FAILED] = "HANDSHAKE_FAILED",
    [EVENT_READ_PIDS_REQUEST] = "READ_PIDS_REQUEST",
    [EVENT_READ_DTCS_REQUEST] = "READ_DTCS_REQUEST",
    [EVENT_CLEAR_DTCS_REQUEST] = "CLEAR_DTCS_REQUEST",
    [EVENT_READ_FREEZE_FRAME_REQUEST] = "READ_FREEZE_FRAME_REQUEST",
    [EVENT_READ_VEHICLE_INFO_REQUEST] = "READ_VEHICLE_INFO_REQUEST",
    [EVENT_OPERATION_COMPLETE] = "OPERATION_COMPLETE",
    [EVENT_OPERATION_FAILED] = "OPERATION_FAILED",
    [EVENT_TIMEOUT] = "TIMEOUT",
    [EVENT_ERROR] = "ERROR",
    [EVENT_RECOVERY_COMPLETE] = "RECOVERY_COMPLETE",
    [EVENT_RECOVERY_FAILED] = "RECOVERY_FAILED"
};

static const StateTransition_t transition_table[] = {
    {STATE_DISCONNECTED, EVENT_CONNECT_REQUEST, STATE_CONNECTING},
    
    {STATE_CONNECTING, EVENT_CONNECTED, STATE_ELM_INIT},
    {STATE_CONNECTING, EVENT_TIMEOUT, STATE_ERROR},
    {STATE_CONNECTING, EVENT_ERROR, STATE_ERROR},
    {STATE_CONNECTING, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED},
    
    {STATE_ELM_INIT, EVENT_ELM_INIT_COMPLETE, STATE_PROTOCOL_DETECT},
    {STATE_ELM_INIT, EVENT_ELM_INIT_FAILED, STATE_RECOVERY},
    {STATE_ELM_INIT, EVENT_TIMEOUT, STATE_RECOVERY},
    {STATE_ELM_INIT, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED},
    
    {STATE_PROTOCOL_DETECT, EVENT_PROTOCOL_DETECTED, STATE_VEHICLE_HANDSHAKE},
    {STATE_PROTOCOL_DETECT, EVENT_PROTOCOL_FAILED, STATE_RECOVERY},
    {STATE_PROTOCOL_DETECT, EVENT_TIMEOUT, STATE_RECOVERY},
    {STATE_PROTOCOL_DETECT, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED},
    
    {STATE_VEHICLE_HANDSHAKE, EVENT_HANDSHAKE_COMPLETE, STATE_IDLE},
    {STATE_VEHICLE_HANDSHAKE, EVENT_HANDSHAKE_FAILED, STATE_RECOVERY},
    {STATE_VEHICLE_HANDSHAKE, EVENT_TIMEOUT, STATE_RECOVERY},
    {STATE_VEHICLE_HANDSHAKE, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED},
    
    {STATE_IDLE, EVENT_READ_PIDS_REQUEST, STATE_READING_PIDS},
    {STATE_IDLE, EVENT_READ_DTCS_REQUEST, STATE_READING_DTCS},
    {STATE_IDLE, EVENT_CLEAR_DTCS_REQUEST, STATE_CLEARING_DTCS},
    {STATE_IDLE, EVENT_READ_FREEZE_FRAME_REQUEST, STATE_READING_FREEZE_FRAME},
    {STATE_IDLE, EVENT_READ_VEHICLE_INFO_REQUEST, STATE_READING_VEHICLE_INFO},
    {STATE_IDLE, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED},
    {STATE_IDLE, EVENT_ERROR, STATE_ERROR},
    
    {STATE_READING_PIDS, EVENT_OPERATION_COMPLETE, STATE_IDLE},
    {STATE_READING_PIDS, EVENT_OPERATION_FAILED, STATE_RECOVERY},
    {STATE_READING_PIDS, EVENT_TIMEOUT, STATE_RECOVERY},
    {STATE_READING_PIDS, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED},
    
    {STATE_READING_DTCS, EVENT_OPERATION_COMPLETE, STATE_IDLE},
    {STATE_READING_DTCS, EVENT_OPERATION_FAILED, STATE_RECOVERY},
    {STATE_READING_DTCS, EVENT_TIMEOUT, STATE_RECOVERY},
    {STATE_READING_DTCS, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED},
    
    {STATE_CLEARING_DTCS, EVENT_OPERATION_COMPLETE, STATE_IDLE},
    {STATE_CLEARING_DTCS, EVENT_OPERATION_FAILED, STATE_RECOVERY},
    {STATE_CLEARING_DTCS, EVENT_TIMEOUT, STATE_RECOVERY},
    {STATE_CLEARING_DTCS, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED},
    
    {STATE_READING_FREEZE_FRAME, EVENT_OPERATION_COMPLETE, STATE_IDLE},
    {STATE_READING_FREEZE_FRAME, EVENT_OPERATION_FAILED, STATE_RECOVERY},
    {STATE_READING_FREEZE_FRAME, EVENT_TIMEOUT, STATE_RECOVERY},
    {STATE_READING_FREEZE_FRAME, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED},
    
    {STATE_READING_VEHICLE_INFO, EVENT_OPERATION_COMPLETE, STATE_IDLE},
    {STATE_READING_VEHICLE_INFO, EVENT_OPERATION_FAILED, STATE_RECOVERY},
    {STATE_READING_VEHICLE_INFO, EVENT_TIMEOUT, STATE_RECOVERY},
    {STATE_READING_VEHICLE_INFO, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED},
    
    {STATE_ERROR, EVENT_RECOVERY_COMPLETE, STATE_IDLE},
    {STATE_ERROR, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED},
    
    {STATE_RECOVERY, EVENT_RECOVERY_COMPLETE, STATE_ELM_INIT},
    {STATE_RECOVERY, EVENT_RECOVERY_FAILED, STATE_ERROR},
    {STATE_RECOVERY, EVENT_TIMEOUT, STATE_ERROR},
    {STATE_RECOVERY, EVENT_DISCONNECT_REQUEST, STATE_DISCONNECTED}
};

#define TRANSITION_TABLE_SIZE (sizeof(transition_table) / sizeof(transition_table[0]))

static State_t find_next_state(State_t current, Event_t event, bool* found)
{
    *found = false;
    
    for (u32 i = 0U; i < TRANSITION_TABLE_SIZE; i++) {
        if ((transition_table[i].from_state == current) && 
            (transition_table[i].event == event)) {
            *found = true;
            return transition_table[i].to_state;
        }
    }
    
    return current;
}

static void execute_transition(StateMachine_t* sm, State_t new_state, Event_t event)
{
    if (sm->state_configs != NULL_PTR) {
        const StateConfig_t* current_config = &sm->state_configs[sm->current_state];
        if (current_config->on_exit != NULL_PTR) {
            current_config->on_exit(sm->context);
        }
    }
    
    sm->previous_state = sm->current_state;
    sm->current_state = new_state;
    sm->retry_count = 0U;
    
    if (sm->get_timestamp_ms != NULL_PTR) {
        sm->state_entry_time_ms = sm->get_timestamp_ms();
    }
    
    if (sm->transition_callback != NULL_PTR) {
        sm->transition_callback(sm->previous_state, sm->current_state, event, sm->context);
    }
    
    if (sm->state_configs != NULL_PTR) {
        const StateConfig_t* new_config = &sm->state_configs[sm->current_state];
        if (new_config->on_entry != NULL_PTR) {
            new_config->on_entry(sm->context);
        }
    }
}

Result_t StateMachine_Init(StateMachine_t* sm, const StateMachineConfig_t* config)
{
    if (sm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    sm->current_state = STATE_DISCONNECTED;
    sm->previous_state = STATE_DISCONNECTED;
    sm->state_entry_time_ms = 0U;
    sm->retry_count = 0U;
    sm->context = config->context;
    sm->transition_callback = config->transition_callback;
    sm->get_timestamp_ms = config->get_timestamp_ms;
    sm->state_configs = config->state_configs;
    sm->error_handler = config->error_handler;
    sm->initialized = true;
    
    if (sm->get_timestamp_ms != NULL_PTR) {
        sm->state_entry_time_ms = sm->get_timestamp_ms();
    }
    
    return RESULT_OK;
}

Result_t StateMachine_ProcessEvent(StateMachine_t* sm, Event_t event)
{
    if (sm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (event >= EVENT_MAX) {
        return RESULT_INVALID_PARAM;
    }
    
    if (event == EVENT_NONE) {
        return RESULT_OK;
    }
    
    bool found = false;
    State_t next_state = find_next_state(sm->current_state, event, &found);
    
    if (found == false) {
        if (sm->error_handler != NULL_PTR) {
            ERROR_REPORT(sm->error_handler, ERR_STATE_INVALID_TRANSITION, ERR_SEV_WARNING);
        }
        return RESULT_ERROR;
    }
    
    execute_transition(sm, next_state, event);
    
    return RESULT_OK;
}

Result_t StateMachine_Update(StateMachine_t* sm)
{
    if (sm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    if (StateMachine_IsTimedOut(sm) == true) {
        if (sm->state_configs != NULL_PTR) {
            const StateConfig_t* config = &sm->state_configs[sm->current_state];
            
            if (sm->retry_count < config->max_retries) {
                sm->retry_count++;
                if (sm->get_timestamp_ms != NULL_PTR) {
                    sm->state_entry_time_ms = sm->get_timestamp_ms();
                }
            } else {
                Result_t result = StateMachine_ProcessEvent(sm, EVENT_TIMEOUT);
                UNUSED(result);
            }
        }
    }
    
    return RESULT_OK;
}

State_t StateMachine_GetCurrentState(const StateMachine_t* sm)
{
    if (sm == NULL_PTR) {
        return STATE_DISCONNECTED;
    }
    
    if (sm->initialized == false) {
        return STATE_DISCONNECTED;
    }
    
    return sm->current_state;
}

State_t StateMachine_GetPreviousState(const StateMachine_t* sm)
{
    if (sm == NULL_PTR) {
        return STATE_DISCONNECTED;
    }
    
    if (sm->initialized == false) {
        return STATE_DISCONNECTED;
    }
    
    return sm->previous_state;
}

bool StateMachine_IsInState(const StateMachine_t* sm, State_t state)
{
    if (sm == NULL_PTR) {
        return false;
    }
    
    if (sm->initialized == false) {
        return false;
    }
    
    return (sm->current_state == state);
}

u32 StateMachine_GetTimeInState(const StateMachine_t* sm)
{
    if (sm == NULL_PTR) {
        return 0U;
    }
    
    if (sm->initialized == false) {
        return 0U;
    }
    
    if (sm->get_timestamp_ms == NULL_PTR) {
        return 0U;
    }
    
    u32 current_time = sm->get_timestamp_ms();
    
    if (current_time >= sm->state_entry_time_ms) {
        return current_time - sm->state_entry_time_ms;
    }
    
    return (0xFFFFFFFFU - sm->state_entry_time_ms) + current_time + 1U;
}

bool StateMachine_IsTimedOut(const StateMachine_t* sm)
{
    if (sm == NULL_PTR) {
        return false;
    }
    
    if (sm->initialized == false) {
        return false;
    }
    
    if (sm->state_configs == NULL_PTR) {
        return false;
    }
    
    const StateConfig_t* config = &sm->state_configs[sm->current_state];
    
    if (config->timeout_ms == 0U) {
        return false;
    }
    
    u32 time_in_state = StateMachine_GetTimeInState(sm);
    
    return (time_in_state >= config->timeout_ms);
}

Result_t StateMachine_Reset(StateMachine_t* sm)
{
    if (sm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }
    
    if (sm->initialized == false) {
        return RESULT_NOT_READY;
    }
    
    execute_transition(sm, STATE_DISCONNECTED, EVENT_DISCONNECT_REQUEST);
    
    return RESULT_OK;
}

const char* StateMachine_GetStateString(State_t state)
{
    if (state >= STATE_MAX) {
        return "UNKNOWN";
    }
    
    return state_strings[state];
}

const char* StateMachine_GetEventString(Event_t event)
{
    if (event >= EVENT_MAX) {
        return "UNKNOWN";
    }
    
    return event_strings[event];
}

bool StateMachine_CanTransition(const StateMachine_t* sm, Event_t event)
{
    if (sm == NULL_PTR) {
        return false;
    }
    
    if (sm->initialized == false) {
        return false;
    }
    
    if (event >= EVENT_MAX) {
        return false;
    }
    
    bool found = false;
    (void)find_next_state(sm->current_state, event, &found);
    
    return found;
}
