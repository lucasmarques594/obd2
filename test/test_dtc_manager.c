#include "unity/unity.h"
#include "../core/dtc/dtc_manager.h"

static DtcManager_t dm;

void setUp(void)
{
    DtcManagerConfig_t cfg = {
        .error_handler = NULL,
        .dtc_callback = NULL,
        .cleared_callback = NULL,
        .callback_context = NULL,
        .get_timestamp_ms = NULL
    };
    DtcManager_Init(&dm, &cfg);
}

void tearDown(void) {}

void test_code_to_string_powertrain(void)
{
    char buf[6];
    TEST_ASSERT_EQUAL(RESULT_OK, DtcManager_CodeToString(0x0301U, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("P0301", buf);
}

void test_code_to_string_chassis(void)
{
    char buf[6];
    TEST_ASSERT_EQUAL(RESULT_OK, DtcManager_CodeToString(0x4101U, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("C0101", buf);
}

void test_code_to_string_body(void)
{
    char buf[6];
    TEST_ASSERT_EQUAL(RESULT_OK, DtcManager_CodeToString(0x8123U, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("B0123", buf);
}

void test_code_to_string_network(void)
{
    char buf[6];
    TEST_ASSERT_EQUAL(RESULT_OK, DtcManager_CodeToString(0xC100U, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("U0100", buf);
}

void test_parse_dtc_powertrain(void)
{
    Dtc_t dtc;
    TEST_ASSERT_EQUAL(RESULT_OK, DtcManager_ParseDtc(0x0301U, &dtc));
    TEST_ASSERT_TRUE(dtc.valid);
    TEST_ASSERT_EQUAL(DTC_SYSTEM_POWERTRAIN, dtc.system);
    TEST_ASSERT_EQUAL_STRING("P0301", dtc.code_string);
    TEST_ASSERT_EQUAL_UINT16(0x0301U, dtc.raw_code);
}

void test_parse_dtc_zero_is_invalid(void)
{
    Dtc_t dtc;
    TEST_ASSERT_EQUAL(RESULT_NO_DATA, DtcManager_ParseDtc(0x0000U, &dtc));
    TEST_ASSERT_FALSE(dtc.valid);
}

void test_parse_dtc_rejects_null(void)
{
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, DtcManager_ParseDtc(0x0301U, NULL));
}

void test_process_response_single_dtc(void)
{
    u8 data[] = { 0x03U, 0x01U };

    TEST_ASSERT_EQUAL(RESULT_OK,
        DtcManager_ProcessResponse(&dm, data, sizeof(data), DTC_TYPE_CURRENT));

    TEST_ASSERT_EQUAL_UINT8(1U, DtcManager_GetCount(&dm, DTC_TYPE_CURRENT));

    Dtc_t dtc;
    TEST_ASSERT_EQUAL(RESULT_OK, DtcManager_GetDtc(&dm, DTC_TYPE_CURRENT, 0U, &dtc));
    TEST_ASSERT_EQUAL_STRING("P0301", dtc.code_string);
}

void test_process_response_multiple_dtcs(void)
{
    u8 data[] = { 0x03U, 0x01U, 0x01U, 0x00U, 0xC1U, 0x00U };

    TEST_ASSERT_EQUAL(RESULT_OK,
        DtcManager_ProcessResponse(&dm, data, sizeof(data), DTC_TYPE_CURRENT));

    TEST_ASSERT_EQUAL_UINT8(3U, DtcManager_GetCount(&dm, DTC_TYPE_CURRENT));
}

void test_process_response_skips_zero_codes(void)
{
    u8 data[] = { 0x00U, 0x00U, 0x03U, 0x01U, 0x00U, 0x00U };

    DtcManager_ProcessResponse(&dm, data, sizeof(data), DTC_TYPE_CURRENT);

    TEST_ASSERT_EQUAL_UINT8(1U, DtcManager_GetCount(&dm, DTC_TYPE_CURRENT));
}

void test_process_pending_dtcs(void)
{
    u8 data[] = { 0x03U, 0x02U };

    DtcManager_ProcessResponse(&dm, data, sizeof(data), DTC_TYPE_PENDING);

    TEST_ASSERT_EQUAL_UINT8(1U, DtcManager_GetCount(&dm, DTC_TYPE_PENDING));
    TEST_ASSERT_EQUAL_UINT8(0U, DtcManager_GetCount(&dm, DTC_TYPE_CURRENT));
}

void test_clear_resets_counts(void)
{
    u8 data[] = { 0x03U, 0x01U };
    DtcManager_ProcessResponse(&dm, data, sizeof(data), DTC_TYPE_CURRENT);
    DtcManager_ProcessResponse(&dm, data, sizeof(data), DTC_TYPE_PENDING);

    TEST_ASSERT_EQUAL(RESULT_OK, DtcManager_Clear(&dm));

    TEST_ASSERT_EQUAL_UINT8(0U, DtcManager_GetCount(&dm, DTC_TYPE_CURRENT));
    TEST_ASSERT_EQUAL_UINT8(0U, DtcManager_GetCount(&dm, DTC_TYPE_PENDING));
    TEST_ASSERT_FALSE(DtcManager_GetMilStatus(&dm));
}

void test_mil_status(void)
{
    TEST_ASSERT_FALSE(DtcManager_GetMilStatus(&dm));

    DtcManager_SetMilStatus(&dm, true, 3U);
    TEST_ASSERT_TRUE(DtcManager_GetMilStatus(&dm));
}

void test_total_count(void)
{
    u8 current[] = { 0x03U, 0x01U };
    u8 pending[] = { 0x01U, 0x00U };

    DtcManager_ProcessResponse(&dm, current, sizeof(current), DTC_TYPE_CURRENT);
    DtcManager_ProcessResponse(&dm, pending, sizeof(pending), DTC_TYPE_PENDING);

    TEST_ASSERT_EQUAL_UINT8(2U, DtcManager_GetTotalCount(&dm));
}

void test_get_all_dtcs(void)
{
    u8 data[] = { 0x03U, 0x01U, 0x01U, 0x00U };
    DtcManager_ProcessResponse(&dm, data, sizeof(data), DTC_TYPE_CURRENT);

    Dtc_t dtcs[8];
    u8 count = 0U;

    TEST_ASSERT_EQUAL(RESULT_OK,
        DtcManager_GetAllDtcs(&dm, DTC_TYPE_CURRENT, dtcs, 8U, &count));

    TEST_ASSERT_EQUAL_UINT8(2U, count);
    TEST_ASSERT_EQUAL_STRING("P0301", dtcs[0].code_string);
    TEST_ASSERT_EQUAL_STRING("P0100", dtcs[1].code_string);
}

void test_type_and_system_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("Current", DtcManager_GetTypeString(DTC_TYPE_CURRENT));
    TEST_ASSERT_EQUAL_STRING("Pending", DtcManager_GetTypeString(DTC_TYPE_PENDING));
    TEST_ASSERT_EQUAL_STRING("Powertrain", DtcManager_GetSystemString(DTC_SYSTEM_POWERTRAIN));
    TEST_ASSERT_EQUAL_STRING("Network", DtcManager_GetSystemString(DTC_SYSTEM_NETWORK));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_code_to_string_powertrain);
    RUN_TEST(test_code_to_string_chassis);
    RUN_TEST(test_code_to_string_body);
    RUN_TEST(test_code_to_string_network);
    RUN_TEST(test_parse_dtc_powertrain);
    RUN_TEST(test_parse_dtc_zero_is_invalid);
    RUN_TEST(test_parse_dtc_rejects_null);
    RUN_TEST(test_process_response_single_dtc);
    RUN_TEST(test_process_response_multiple_dtcs);
    RUN_TEST(test_process_response_skips_zero_codes);
    RUN_TEST(test_process_pending_dtcs);
    RUN_TEST(test_clear_resets_counts);
    RUN_TEST(test_mil_status);
    RUN_TEST(test_total_count);
    RUN_TEST(test_get_all_dtcs);
    RUN_TEST(test_type_and_system_strings);

    return UNITY_END();
}
