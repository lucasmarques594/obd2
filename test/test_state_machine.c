#include "unity/unity.h"
#include "../core/state_machine/state_machine.h"

static StateMachine_t sm;
static State_t last_from;
static State_t last_to;
static Event_t last_event;

static void transition_cb(State_t from, State_t to, Event_t event, void* context)
{
    UNUSED(context);
    last_from = from;
    last_to = to;
    last_event = event;
}

void setUp(void)
{
    last_from = STATE_MAX;
    last_to = STATE_MAX;
    last_event = EVENT_MAX;

    StateMachineConfig_t cfg = {
        .context = NULL,
        .transition_callback = transition_cb,
        .get_timestamp_ms = NULL,
        .state_configs = NULL,
        .error_handler = NULL
    };
    StateMachine_Init(&sm, &cfg);
}

void tearDown(void) {}

void test_init_starts_disconnected(void)
{
    TEST_ASSERT_EQUAL(STATE_DISCONNECTED, StateMachine_GetCurrentState(&sm));
}

void test_connect_request(void)
{
    TEST_ASSERT_EQUAL(RESULT_OK, StateMachine_ProcessEvent(&sm, EVENT_CONNECT_REQUEST));
    TEST_ASSERT_EQUAL(STATE_CONNECTING, StateMachine_GetCurrentState(&sm));
    TEST_ASSERT_EQUAL(STATE_DISCONNECTED, last_from);
    TEST_ASSERT_EQUAL(STATE_CONNECTING, last_to);
}

void test_full_connect_sequence(void)
{
    StateMachine_ProcessEvent(&sm, EVENT_CONNECT_REQUEST);
    TEST_ASSERT_EQUAL(STATE_CONNECTING, StateMachine_GetCurrentState(&sm));

    StateMachine_ProcessEvent(&sm, EVENT_CONNECTED);
    TEST_ASSERT_EQUAL(STATE_ELM_INIT, StateMachine_GetCurrentState(&sm));

    StateMachine_ProcessEvent(&sm, EVENT_ELM_INIT_COMPLETE);
    TEST_ASSERT_EQUAL(STATE_PROTOCOL_DETECT, StateMachine_GetCurrentState(&sm));

    StateMachine_ProcessEvent(&sm, EVENT_PROTOCOL_DETECTED);
    TEST_ASSERT_EQUAL(STATE_VEHICLE_HANDSHAKE, StateMachine_GetCurrentState(&sm));

    StateMachine_ProcessEvent(&sm, EVENT_HANDSHAKE_COMPLETE);
    TEST_ASSERT_EQUAL(STATE_IDLE, StateMachine_GetCurrentState(&sm));
}

void test_idle_to_reading_pids(void)
{
    StateMachine_ProcessEvent(&sm, EVENT_CONNECT_REQUEST);
    StateMachine_ProcessEvent(&sm, EVENT_CONNECTED);
    StateMachine_ProcessEvent(&sm, EVENT_ELM_INIT_COMPLETE);
    StateMachine_ProcessEvent(&sm, EVENT_PROTOCOL_DETECTED);
    StateMachine_ProcessEvent(&sm, EVENT_HANDSHAKE_COMPLETE);

    TEST_ASSERT_EQUAL(RESULT_OK, StateMachine_ProcessEvent(&sm, EVENT_READ_PIDS_REQUEST));
    TEST_ASSERT_EQUAL(STATE_READING_PIDS, StateMachine_GetCurrentState(&sm));

    StateMachine_ProcessEvent(&sm, EVENT_OPERATION_COMPLETE);
    TEST_ASSERT_EQUAL(STATE_IDLE, StateMachine_GetCurrentState(&sm));
}

void test_invalid_transition_returns_error(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, StateMachine_ProcessEvent(&sm, EVENT_CONNECTED));
    TEST_ASSERT_EQUAL(STATE_DISCONNECTED, StateMachine_GetCurrentState(&sm));
}

void test_disconnect_from_any_state(void)
{
    StateMachine_ProcessEvent(&sm, EVENT_CONNECT_REQUEST);
    StateMachine_ProcessEvent(&sm, EVENT_CONNECTED);
    TEST_ASSERT_EQUAL(STATE_ELM_INIT, StateMachine_GetCurrentState(&sm));

    TEST_ASSERT_EQUAL(RESULT_OK, StateMachine_ProcessEvent(&sm, EVENT_DISCONNECT_REQUEST));
    TEST_ASSERT_EQUAL(STATE_DISCONNECTED, StateMachine_GetCurrentState(&sm));
}

void test_error_recovery_flow(void)
{
    StateMachine_ProcessEvent(&sm, EVENT_CONNECT_REQUEST);
    StateMachine_ProcessEvent(&sm, EVENT_CONNECTED);

    StateMachine_ProcessEvent(&sm, EVENT_ELM_INIT_FAILED);
    TEST_ASSERT_EQUAL(STATE_RECOVERY, StateMachine_GetCurrentState(&sm));

    StateMachine_ProcessEvent(&sm, EVENT_RECOVERY_COMPLETE);
    TEST_ASSERT_EQUAL(STATE_ELM_INIT, StateMachine_GetCurrentState(&sm));
}

void test_recovery_failure_goes_to_error(void)
{
    StateMachine_ProcessEvent(&sm, EVENT_CONNECT_REQUEST);
    StateMachine_ProcessEvent(&sm, EVENT_CONNECTED);
    StateMachine_ProcessEvent(&sm, EVENT_ELM_INIT_FAILED);

    StateMachine_ProcessEvent(&sm, EVENT_RECOVERY_FAILED);
    TEST_ASSERT_EQUAL(STATE_ERROR, StateMachine_GetCurrentState(&sm));
}

void test_can_transition(void)
{
    TEST_ASSERT_TRUE(StateMachine_CanTransition(&sm, EVENT_CONNECT_REQUEST));
    TEST_ASSERT_FALSE(StateMachine_CanTransition(&sm, EVENT_CONNECTED));
    TEST_ASSERT_FALSE(StateMachine_CanTransition(&sm, EVENT_READ_PIDS_REQUEST));
}

void test_previous_state(void)
{
    StateMachine_ProcessEvent(&sm, EVENT_CONNECT_REQUEST);
    StateMachine_ProcessEvent(&sm, EVENT_CONNECTED);

    TEST_ASSERT_EQUAL(STATE_CONNECTING, StateMachine_GetPreviousState(&sm));
    TEST_ASSERT_EQUAL(STATE_ELM_INIT, StateMachine_GetCurrentState(&sm));
}

void test_reset(void)
{
    StateMachine_ProcessEvent(&sm, EVENT_CONNECT_REQUEST);
    StateMachine_ProcessEvent(&sm, EVENT_CONNECTED);

    TEST_ASSERT_EQUAL(RESULT_OK, StateMachine_Reset(&sm));
    TEST_ASSERT_EQUAL(STATE_DISCONNECTED, StateMachine_GetCurrentState(&sm));
}

void test_state_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("DISCONNECTED", StateMachine_GetStateString(STATE_DISCONNECTED));
    TEST_ASSERT_EQUAL_STRING("IDLE", StateMachine_GetStateString(STATE_IDLE));
    TEST_ASSERT_EQUAL_STRING("ERROR", StateMachine_GetStateString(STATE_ERROR));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", StateMachine_GetStateString(STATE_MAX));
}

void test_event_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("CONNECT_REQUEST", StateMachine_GetEventString(EVENT_CONNECT_REQUEST));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", StateMachine_GetEventString(EVENT_MAX));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_starts_disconnected);
    RUN_TEST(test_connect_request);
    RUN_TEST(test_full_connect_sequence);
    RUN_TEST(test_idle_to_reading_pids);
    RUN_TEST(test_invalid_transition_returns_error);
    RUN_TEST(test_disconnect_from_any_state);
    RUN_TEST(test_error_recovery_flow);
    RUN_TEST(test_recovery_failure_goes_to_error);
    RUN_TEST(test_can_transition);
    RUN_TEST(test_previous_state);
    RUN_TEST(test_reset);
    RUN_TEST(test_state_strings);
    RUN_TEST(test_event_strings);

    return UNITY_END();
}
