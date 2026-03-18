#include "unity/unity.h"
#include "../core/error/error_handler.h"

static ErrorHandler_t handler;
static ErrorInfo_t last_error;
static bool callback_fired;

static void test_error_cb(const ErrorInfo_t* error)
{
    last_error = *error;
    callback_fired = true;
}

void setUp(void)
{
    callback_fired = false;
    ErrorHandler_Init(&handler, test_error_cb);
}

void tearDown(void) {}

void test_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, ErrorHandler_Init(NULL, test_error_cb));
}

void test_init_accepts_null_callback(void)
{
    ErrorHandler_t h;
    TEST_ASSERT_EQUAL(RESULT_OK, ErrorHandler_Init(&h, NULL));
}

void test_report_fires_callback(void)
{
    ErrorHandler_Report(&handler, ERR_COMM_TIMEOUT, ERR_SEV_WARNING, "test.c", 42U, "test_func");

    TEST_ASSERT_TRUE(callback_fired);
    TEST_ASSERT_EQUAL(ERR_COMM_TIMEOUT, last_error.code);
    TEST_ASSERT_EQUAL(ERR_SEV_WARNING, last_error.severity);
    TEST_ASSERT_EQUAL_UINT32(42U, last_error.line);
}

void test_report_clamps_invalid_code(void)
{
    ErrorHandler_Report(&handler, (ErrorCode_t)9999, ERR_SEV_ERROR, "test.c", 1U, "fn");

    TEST_ASSERT_TRUE(callback_fired);
    TEST_ASSERT_EQUAL(ERR_UNKNOWN, last_error.code);
}

void test_report_clamps_invalid_severity(void)
{
    ErrorHandler_Report(&handler, ERR_NONE, (ErrorSeverity_t)99, "test.c", 1U, "fn");

    TEST_ASSERT_TRUE(callback_fired);
    TEST_ASSERT_EQUAL(ERR_SEV_ERROR, last_error.severity);
}

void test_report_without_callback(void)
{
    ErrorHandler_t h;
    ErrorHandler_Init(&h, NULL);

    TEST_ASSERT_EQUAL(RESULT_OK,
        ErrorHandler_Report(&h, ERR_COMM_TIMEOUT, ERR_SEV_INFO, "test.c", 1U, "fn"));
}

void test_code_string_known(void)
{
    TEST_ASSERT_EQUAL_STRING("ERR_NONE", ErrorHandler_GetCodeString(ERR_NONE));
    TEST_ASSERT_EQUAL_STRING("ERR_COMM_TIMEOUT", ErrorHandler_GetCodeString(ERR_COMM_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("ERR_ELM_NO_DATA", ErrorHandler_GetCodeString(ERR_ELM_NO_DATA));
    TEST_ASSERT_EQUAL_STRING("ERR_OBD_FRAME_ERROR", ErrorHandler_GetCodeString(ERR_OBD_FRAME_ERROR));
    TEST_ASSERT_EQUAL_STRING("ERR_SCHEDULER_TASK_NOT_FOUND", ErrorHandler_GetCodeString(ERR_SCHEDULER_TASK_NOT_FOUND));
    TEST_ASSERT_EQUAL_STRING("ERR_UNKNOWN", ErrorHandler_GetCodeString(ERR_UNKNOWN));
}

void test_code_string_unknown(void)
{
    TEST_ASSERT_EQUAL_STRING("ERR_UNKNOWN", ErrorHandler_GetCodeString((ErrorCode_t)9999));
    TEST_ASSERT_EQUAL_STRING("ERR_UNKNOWN", ErrorHandler_GetCodeString((ErrorCode_t)150));
}

void test_severity_strings(void)
{
    TEST_ASSERT_EQUAL_STRING("INFO", ErrorHandler_GetSeverityString(ERR_SEV_INFO));
    TEST_ASSERT_EQUAL_STRING("WARNING", ErrorHandler_GetSeverityString(ERR_SEV_WARNING));
    TEST_ASSERT_EQUAL_STRING("ERROR", ErrorHandler_GetSeverityString(ERR_SEV_ERROR));
    TEST_ASSERT_EQUAL_STRING("CRITICAL", ErrorHandler_GetSeverityString(ERR_SEV_CRITICAL));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", ErrorHandler_GetSeverityString(ERR_SEV_MAX));
}

void test_recoverable_true(void)
{
    TEST_ASSERT_TRUE(ErrorHandler_IsRecoverable(ERR_COMM_TIMEOUT));
    TEST_ASSERT_TRUE(ErrorHandler_IsRecoverable(ERR_ELM_NO_DATA));
    TEST_ASSERT_TRUE(ErrorHandler_IsRecoverable(ERR_OBD_CHECKSUM_FAILED));
    TEST_ASSERT_TRUE(ErrorHandler_IsRecoverable(ERR_SCHEDULER_QUEUE_FULL));
}

void test_recoverable_false(void)
{
    TEST_ASSERT_FALSE(ErrorHandler_IsRecoverable(ERR_COMM_DISCONNECTED));
    TEST_ASSERT_FALSE(ErrorHandler_IsRecoverable(ERR_OBD_INVALID_MODE));
    TEST_ASSERT_FALSE(ErrorHandler_IsRecoverable(ERR_MEMORY_ALLOCATION));
    TEST_ASSERT_FALSE(ErrorHandler_IsRecoverable(ERR_PARAM_NULL_POINTER));
    TEST_ASSERT_FALSE(ErrorHandler_IsRecoverable(ERR_UNKNOWN));
}

void test_recoverable_unknown_code(void)
{
    TEST_ASSERT_FALSE(ErrorHandler_IsRecoverable((ErrorCode_t)9999));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_rejects_null);
    RUN_TEST(test_init_accepts_null_callback);
    RUN_TEST(test_report_fires_callback);
    RUN_TEST(test_report_clamps_invalid_code);
    RUN_TEST(test_report_clamps_invalid_severity);
    RUN_TEST(test_report_without_callback);
    RUN_TEST(test_code_string_known);
    RUN_TEST(test_code_string_unknown);
    RUN_TEST(test_severity_strings);
    RUN_TEST(test_recoverable_true);
    RUN_TEST(test_recoverable_false);
    RUN_TEST(test_recoverable_unknown_code);

    return UNITY_END();
}

