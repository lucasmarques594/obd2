#include "unity/unity.h"
#include "../core/pid/pid_manager.h"

static PidManager_t pm;

void setUp(void)
{
    PidManagerConfig_t cfg = {
        .error_handler = NULL,
        .value_callback = NULL,
        .callback_context = NULL,
        .get_timestamp_ms = NULL
    };
    PidManager_Init(&pm, &cfg);
}

void tearDown(void) {}

void test_init_rejects_null(void)
{
    PidManagerConfig_t cfg = { .error_handler = NULL };
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, PidManager_Init(NULL, &cfg));
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, PidManager_Init(&pm, NULL));
}

void test_convert_rpm(void)
{
    u8 raw[] = { 0x1AU, 0xF8U };
    PidValue_t value;

    TEST_ASSERT_EQUAL(RESULT_OK, PidManager_ConvertRawToEng(0x0CU, raw, 2U, &value));
    TEST_ASSERT_TRUE(value.valid);
    TEST_ASSERT_EQUAL(PID_UNIT_RPM, value.unit);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 1726.0f, value.eng_value);
}

void test_convert_speed(void)
{
    u8 raw[] = { 0x3CU };
    PidValue_t value;

    TEST_ASSERT_EQUAL(RESULT_OK, PidManager_ConvertRawToEng(0x0DU, raw, 1U, &value));
    TEST_ASSERT_TRUE(value.valid);
    TEST_ASSERT_EQUAL(PID_UNIT_KMH, value.unit);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.0f, value.eng_value);
}

void test_convert_coolant_temp(void)
{
    u8 raw[] = { 0x7BU };
    PidValue_t value;

    TEST_ASSERT_EQUAL(RESULT_OK, PidManager_ConvertRawToEng(0x05U, raw, 1U, &value));
    TEST_ASSERT_TRUE(value.valid);
    TEST_ASSERT_EQUAL(PID_UNIT_DEGREES_C, value.unit);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 83.0f, value.eng_value);
}

void test_convert_coolant_temp_freezing(void)
{
    u8 raw[] = { 0x00U };
    PidValue_t value;

    TEST_ASSERT_EQUAL(RESULT_OK, PidManager_ConvertRawToEng(0x05U, raw, 1U, &value));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -40.0f, value.eng_value);
}

void test_convert_engine_load(void)
{
    u8 raw[] = { 0xFFU };
    PidValue_t value;

    TEST_ASSERT_EQUAL(RESULT_OK, PidManager_ConvertRawToEng(0x04U, raw, 1U, &value));
    TEST_ASSERT_TRUE(value.valid);
    TEST_ASSERT_EQUAL(PID_UNIT_PERCENT, value.unit);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, value.eng_value);
}

void test_convert_throttle_zero(void)
{
    u8 raw[] = { 0x00U };
    PidValue_t value;

    TEST_ASSERT_EQUAL(RESULT_OK, PidManager_ConvertRawToEng(0x11U, raw, 1U, &value));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, value.eng_value);
}

void test_convert_unknown_pid(void)
{
    u8 raw[] = { 0x42U };
    PidValue_t value;

    TEST_ASSERT_EQUAL(RESULT_OK, PidManager_ConvertRawToEng(0xFEU, raw, 1U, &value));
    TEST_ASSERT_TRUE(value.valid);
    TEST_ASSERT_EQUAL(PID_UNIT_NONE, value.unit);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 66.0f, value.eng_value);
}

void test_convert_rejects_null(void)
{
    PidValue_t value;
    u8 raw[] = { 0x00U };

    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, PidManager_ConvertRawToEng(0x0CU, NULL, 1U, &value));
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, PidManager_ConvertRawToEng(0x0CU, raw, 1U, NULL));
}

void test_convert_insufficient_data(void)
{
    u8 raw[] = { 0x1AU };
    PidValue_t value;

    TEST_ASSERT_EQUAL(RESULT_ERROR, PidManager_ConvertRawToEng(0x0CU, raw, 1U, &value));
}

void test_definition_lookup(void)
{
    const PidDefinition_t* def = PidManager_GetDefinition(0x0CU);
    TEST_ASSERT_NOT_NULL(def);
    TEST_ASSERT_EQUAL_UINT8(0x0CU, def->pid);
    TEST_ASSERT_EQUAL_STRING("Engine RPM", def->name);
    TEST_ASSERT_EQUAL(PID_UNIT_RPM, def->unit);
    TEST_ASSERT_EQUAL_UINT8(2U, def->data_bytes);
}

void test_definition_lookup_unknown(void)
{
    TEST_ASSERT_NULL(PidManager_GetDefinition(0xFEU));
}

void test_unit_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("RPM", PidManager_GetUnitString(PID_UNIT_RPM));
    TEST_ASSERT_EQUAL_STRING("km/h", PidManager_GetUnitString(PID_UNIT_KMH));
    TEST_ASSERT_EQUAL_STRING("%", PidManager_GetUnitString(PID_UNIT_PERCENT));
}

void test_set_supported_pids(void)
{
    u8 supported[] = { 0x18U, 0x3EU, 0x00U, 0x00U };

    TEST_ASSERT_EQUAL(RESULT_OK, PidManager_SetSupported(&pm, supported, 0x00U));

    TEST_ASSERT_TRUE(PidManager_IsSupported(&pm, 0x04U));
    TEST_ASSERT_TRUE(PidManager_IsSupported(&pm, 0x05U));
    TEST_ASSERT_FALSE(PidManager_IsSupported(&pm, 0x01U));
}

void test_enable_disable_pid(void)
{
    TEST_ASSERT_EQUAL(RESULT_OK, PidManager_EnablePid(&pm, 0x0CU, 100U));
    TEST_ASSERT_EQUAL_UINT8(1U, PidManager_GetEnabledCount(&pm));

    TEST_ASSERT_EQUAL(RESULT_OK, PidManager_DisablePid(&pm, 0x0CU));
    TEST_ASSERT_EQUAL_UINT8(0U, PidManager_GetEnabledCount(&pm));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_rejects_null);
    RUN_TEST(test_convert_rpm);
    RUN_TEST(test_convert_speed);
    RUN_TEST(test_convert_coolant_temp);
    RUN_TEST(test_convert_coolant_temp_freezing);
    RUN_TEST(test_convert_engine_load);
    RUN_TEST(test_convert_throttle_zero);
    RUN_TEST(test_convert_unknown_pid);
    RUN_TEST(test_convert_rejects_null);
    RUN_TEST(test_convert_insufficient_data);
    RUN_TEST(test_definition_lookup);
    RUN_TEST(test_definition_lookup_unknown);
    RUN_TEST(test_unit_strings);
    RUN_TEST(test_set_supported_pids);
    RUN_TEST(test_enable_disable_pid);

    return UNITY_END();
}
