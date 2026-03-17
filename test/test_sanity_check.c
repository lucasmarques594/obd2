#include "unity/unity.h"
#include "../core/sanity_check/sanity_check.h"

static SanityCheck_t sc;
static u32 mock_time_ms;

static u32 mock_get_timestamp(void)
{
    return mock_time_ms;
}

void setUp(void)
{
    mock_time_ms = 1000U;

    SanityCheckConfig_t cfg = {
        .error_handler = NULL,
        .fail_callback = NULL,
        .callback_context = NULL,
        .get_timestamp_ms = mock_get_timestamp
    };
    SanityCheck_Init(&sc, &cfg);
}

void tearDown(void) {}

void test_range_ok(void)
{
    TEST_ASSERT_EQUAL(SANITY_RESULT_OK, SanityCheck_ValidateRange(0x0CU, 3000.0f));
}

void test_range_too_high(void)
{
    TEST_ASSERT_EQUAL(SANITY_RESULT_OUT_OF_RANGE, SanityCheck_ValidateRange(0x0CU, 20000.0f));
}

void test_range_too_low(void)
{
    TEST_ASSERT_EQUAL(SANITY_RESULT_OUT_OF_RANGE, SanityCheck_ValidateRange(0x05U, -50.0f));
}

void test_range_unknown_pid_passes(void)
{
    TEST_ASSERT_EQUAL(SANITY_RESULT_OK, SanityCheck_ValidateRange(0xFEU, 99999.0f));
}

void test_stuck_sensor_detection(void)
{
    PidValue_t v = { .raw_value = 0, .eng_value = 3000.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };

    /* First call seeds the history — no previous value to compare */
    mock_time_ms = 1000U;
    TEST_ASSERT_EQUAL(SANITY_RESULT_OK, SanityCheck_ValidatePid(&sc, 0x0CU, &v));

    /* Next THRESHOLD calls with same value should trigger stuck on the last one */
    for (u8 i = 0U; i < SANITY_STUCK_THRESHOLD; i++) {
        mock_time_ms = 1100U + (u32)(i * 100U);
        SanityResult_t r = SanityCheck_ValidatePid(&sc, 0x0CU, &v);

        if (i < SANITY_STUCK_THRESHOLD - 1U) {
            TEST_ASSERT_EQUAL(SANITY_RESULT_OK, r);
        } else {
            TEST_ASSERT_EQUAL(SANITY_RESULT_SENSOR_STUCK, r);
        }
    }
}

void test_stuck_resets_on_change(void)
{
    PidValue_t v = { .raw_value = 0, .eng_value = 3000.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };

    for (u8 i = 0U; i < SANITY_STUCK_THRESHOLD - 2U; i++) {
        mock_time_ms = 1000U + (u32)(i * 100U);
        SanityCheck_ValidatePid(&sc, 0x0CU, &v);
    }

    v.eng_value = 3100.0f;
    mock_time_ms = 1400U;
    SanityCheck_ValidatePid(&sc, 0x0CU, &v);

    v.eng_value = 3100.0f;
    for (u8 i = 0U; i < SANITY_STUCK_THRESHOLD - 1U; i++) {
        mock_time_ms = 1500U + (u32)(i * 100U);
        TEST_ASSERT_EQUAL(SANITY_RESULT_OK, SanityCheck_ValidatePid(&sc, 0x0CU, &v));
    }
}

void test_rate_of_change_with_time_normalization(void)
{
    /* RPM max_rate_per_sec = 8000. At 100ms interval, max delta = 800 */
    PidValue_t v1 = { .raw_value = 0, .eng_value = 3000.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };
    mock_time_ms = 1000U;
    SanityCheck_ValidatePid(&sc, 0x0CU, &v1);

    /* 100ms later, jump 700 RPM — should pass (700 < 800 allowed) */
    PidValue_t v2 = { .raw_value = 0, .eng_value = 3700.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };
    mock_time_ms = 1100U;
    TEST_ASSERT_EQUAL(SANITY_RESULT_OK, SanityCheck_ValidatePid(&sc, 0x0CU, &v2));
}

void test_rate_of_change_exceeds_temporal_limit(void)
{
    /* RPM max_rate_per_sec = 8000. At 100ms interval, max delta = 800 */
    PidValue_t v1 = { .raw_value = 0, .eng_value = 3000.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };
    mock_time_ms = 1000U;
    SanityCheck_ValidatePid(&sc, 0x0CU, &v1);

    /* 100ms later, jump 1000 RPM — should fail (1000 > 800 allowed) */
    PidValue_t v2 = { .raw_value = 0, .eng_value = 4000.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };
    mock_time_ms = 1100U;
    TEST_ASSERT_EQUAL(SANITY_RESULT_RATE_OF_CHANGE, SanityCheck_ValidatePid(&sc, 0x0CU, &v2));
}

void test_rate_of_change_larger_interval_allows_more(void)
{
    /* RPM max_rate_per_sec = 8000. At 500ms interval, max delta = 4000 */
    PidValue_t v1 = { .raw_value = 0, .eng_value = 3000.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };
    mock_time_ms = 1000U;
    SanityCheck_ValidatePid(&sc, 0x0CU, &v1);

    /* 500ms later, jump 3500 RPM — should pass (3500 < 4000 allowed) */
    PidValue_t v2 = { .raw_value = 0, .eng_value = 6500.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };
    mock_time_ms = 1500U;
    TEST_ASSERT_EQUAL(SANITY_RESULT_OK, SanityCheck_ValidatePid(&sc, 0x0CU, &v2));
}

void test_rate_of_change_without_timestamp(void)
{
    SanityCheckConfig_t cfg = {
        .error_handler = NULL,
        .fail_callback = NULL,
        .callback_context = NULL,
        .get_timestamp_ms = NULL
    };
    SanityCheck_t sc_no_ts;
    SanityCheck_Init(&sc_no_ts, &cfg);

    /* Without timestamp, falls back to raw max_rate_per_sec comparison */
    /* RPM max_rate_per_sec = 8000. Without time, max_allowed = 8000 absolute */
    PidValue_t v1 = { .raw_value = 0, .eng_value = 3000.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };
    SanityCheck_ValidatePid(&sc_no_ts, 0x0CU, &v1);

    /* Jump 7000 — should pass (7000 < 8000) */
    PidValue_t v2 = { .raw_value = 0, .eng_value = 10000.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };
    TEST_ASSERT_EQUAL(SANITY_RESULT_OK, SanityCheck_ValidatePid(&sc_no_ts, 0x0CU, &v2));

    /* Jump 9000 — should fail (9000 > 8000) */
    PidValue_t v3 = { .raw_value = 0, .eng_value = 1000.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };
    TEST_ASSERT_EQUAL(SANITY_RESULT_RATE_OF_CHANGE, SanityCheck_ValidatePid(&sc_no_ts, 0x0CU, &v3));
}

void test_validate_pid_full_pipeline(void)
{
    mock_time_ms = 1000U;
    PidValue_t v = { .raw_value = 0, .eng_value = 3000.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };
    TEST_ASSERT_EQUAL(SANITY_RESULT_OK, SanityCheck_ValidatePid(&sc, 0x0CU, &v));
    TEST_ASSERT_EQUAL_UINT32(1U, SanityCheck_GetTotalChecks(&sc));
    TEST_ASSERT_EQUAL_UINT32(0U, SanityCheck_GetTotalFailures(&sc));
}

void test_validate_pid_invalid_data(void)
{
    PidValue_t v = { .raw_value = 0, .eng_value = 0.0f, .unit = PID_UNIT_NONE, .timestamp_ms = 0, .valid = false };
    TEST_ASSERT_EQUAL(SANITY_RESULT_INVALID_DATA, SanityCheck_ValidatePid(&sc, 0x0CU, &v));
}

void test_validate_pid_null(void)
{
    TEST_ASSERT_EQUAL(SANITY_RESULT_INVALID_DATA, SanityCheck_ValidatePid(&sc, 0x0CU, NULL));
}

void test_clear_history(void)
{
    PidValue_t v = { .raw_value = 0, .eng_value = 3000.0f, .unit = PID_UNIT_RPM, .timestamp_ms = 0, .valid = true };

    mock_time_ms = 1000U;
    SanityCheck_ValidatePid(&sc, 0x0CU, &v);
    mock_time_ms = 1100U;
    SanityCheck_ValidatePid(&sc, 0x0CU, &v);

    TEST_ASSERT_EQUAL(RESULT_OK, SanityCheck_ClearHistory(&sc, 0x0CU));

    for (u8 i = 0U; i < SANITY_STUCK_THRESHOLD - 1U; i++) {
        mock_time_ms = 1200U + (u32)(i * 100U);
        TEST_ASSERT_EQUAL(SANITY_RESULT_OK, SanityCheck_ValidatePid(&sc, 0x0CU, &v));
    }
}

void test_get_rule(void)
{
    const SanityRule_t* rule = SanityCheck_GetRule(0x0CU);
    TEST_ASSERT_NOT_NULL(rule);
    TEST_ASSERT_EQUAL_UINT8(0x0CU, rule->pid);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 8000.0f, rule->max_rate_per_sec);
}

void test_result_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("OK", SanityCheck_GetResultString(SANITY_RESULT_OK));
    TEST_ASSERT_EQUAL_STRING("Rate of Change Exceeded", SanityCheck_GetResultString(SANITY_RESULT_RATE_OF_CHANGE));
    TEST_ASSERT_EQUAL_STRING("Unknown", SanityCheck_GetResultString(SANITY_RESULT_MAX));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_range_ok);
    RUN_TEST(test_range_too_high);
    RUN_TEST(test_range_too_low);
    RUN_TEST(test_range_unknown_pid_passes);
    RUN_TEST(test_stuck_sensor_detection);
    RUN_TEST(test_stuck_resets_on_change);
    RUN_TEST(test_rate_of_change_with_time_normalization);
    RUN_TEST(test_rate_of_change_exceeds_temporal_limit);
    RUN_TEST(test_rate_of_change_larger_interval_allows_more);
    RUN_TEST(test_rate_of_change_without_timestamp);
    RUN_TEST(test_validate_pid_full_pipeline);
    RUN_TEST(test_validate_pid_invalid_data);
    RUN_TEST(test_validate_pid_null);
    RUN_TEST(test_clear_history);
    RUN_TEST(test_get_rule);
    RUN_TEST(test_result_strings);

    return UNITY_END();
}

