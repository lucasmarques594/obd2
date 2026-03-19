#include "unity/unity.h"
#include "../core/llm/llm_dtc.h"
#include <string.h>

static LlmDtcInterpreter_t llm;

void setUp(void) {}
void tearDown(void) {}

void test_init_groq(void)
{
    TEST_ASSERT_EQUAL(RESULT_OK, LlmDtc_InitGroq(&llm, "gsk_test_key_123"));
    TEST_ASSERT_TRUE(llm.initialized);
    TEST_ASSERT_EQUAL(LLM_PROVIDER_GROQ, llm.config.provider);
    TEST_ASSERT_EQUAL_STRING("gsk_test_key_123", llm.config.api_key);
    TEST_ASSERT_EQUAL_UINT32(0U, llm.total_requests);
}

void test_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, LlmDtc_InitGroq(NULL, "key"));
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, LlmDtc_InitGroq(&llm, NULL));
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, LlmDtc_InitGroq(&llm, ""));
}

void test_init_full_config(void)
{
    LlmConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.api_key, "test_key");
    strcpy(cfg.model, "llama-3.3-70b-versatile");
    strcpy(cfg.base_url, "https://api.groq.com/openai/v1/chat/completions");
    cfg.provider = LLM_PROVIDER_GROQ;
    cfg.timeout_seconds = 15U;
    cfg.temperature = 0.1f;
    cfg.max_tokens = 512U;

    TEST_ASSERT_EQUAL(RESULT_OK, LlmDtc_Init(&llm, &cfg));
    TEST_ASSERT_EQUAL_UINT16(512U, llm.config.max_tokens);
    TEST_ASSERT_EQUAL_UINT32(15U, llm.config.timeout_seconds);
}

void test_init_rejects_empty_key(void)
{
    LlmConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.api_key[0] = '\0';

    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, LlmDtc_Init(&llm, &cfg));
}

void test_set_api_key(void)
{
    LlmDtc_InitGroq(&llm, "old_key");

    TEST_ASSERT_EQUAL(RESULT_OK, LlmDtc_SetApiKey(&llm, "new_key_456"));
    TEST_ASSERT_EQUAL_STRING("new_key_456", llm.config.api_key);
}

void test_set_model(void)
{
    LlmDtc_InitGroq(&llm, "key");

    TEST_ASSERT_EQUAL(RESULT_OK, LlmDtc_SetModel(&llm, "llama-3.1-8b-instant"));
    TEST_ASSERT_EQUAL_STRING("llama-3.1-8b-instant", llm.config.model);
}

void test_interpret_single_not_initialized(void)
{
    LlmDtcInterpreter_t uninit;
    memset(&uninit, 0, sizeof(uninit));
    uninit.initialized = false;

    LlmDtcResult_t result;
    TEST_ASSERT_EQUAL(RESULT_NOT_READY, LlmDtc_InterpretSingle(&uninit, "P0301", &result));
}

void test_interpret_single_null_params(void)
{
    LlmDtc_InitGroq(&llm, "key");

    LlmDtcResult_t result;
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, LlmDtc_InterpretSingle(NULL, "P0301", &result));
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, LlmDtc_InterpretSingle(&llm, NULL, &result));
    TEST_ASSERT_EQUAL(RESULT_INVALID_PARAM, LlmDtc_InterpretSingle(&llm, "P0301", NULL));
}

void test_interpret_multiple_empty(void)
{
    LlmDtc_InitGroq(&llm, "key");

    Dtc_t empty_dtcs[1];
    LlmResponse_t response;
    TEST_ASSERT_EQUAL(RESULT_NO_DATA, LlmDtc_InterpretMultiple(&llm, empty_dtcs, 0U, &response));
}

void test_interpret_from_manager_no_dtcs(void)
{
    LlmDtc_InitGroq(&llm, "key");

    DtcManager_t dm;
    DtcManagerConfig_t dm_cfg = {
        .error_handler = NULL,
        .dtc_callback = NULL,
        .cleared_callback = NULL,
        .callback_context = NULL,
        .get_timestamp_ms = NULL
    };
    DtcManager_Init(&dm, &dm_cfg);

    LlmResponse_t response;
    TEST_ASSERT_EQUAL(RESULT_NO_DATA, LlmDtc_InterpretFromManager(&llm, &dm, DTC_TYPE_CURRENT, &response));
}

void test_provider_string(void)
{
    TEST_ASSERT_EQUAL_STRING("Groq", LlmDtc_GetProviderString(LLM_PROVIDER_GROQ));
    TEST_ASSERT_EQUAL_STRING("Unknown", LlmDtc_GetProviderString(LLM_PROVIDER_MAX));
}

void test_default_groq_config(void)
{
    LlmDtc_InitGroq(&llm, "gsk_abc");

    TEST_ASSERT_EQUAL_STRING("llama-3.3-70b-versatile", llm.config.model);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.2f, llm.config.temperature);
    TEST_ASSERT_EQUAL_UINT16(1024U, llm.config.max_tokens);
    TEST_ASSERT_EQUAL_UINT32(30U, llm.config.timeout_seconds);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_groq);
    RUN_TEST(test_init_rejects_null);
    RUN_TEST(test_init_full_config);
    RUN_TEST(test_init_rejects_empty_key);
    RUN_TEST(test_set_api_key);
    RUN_TEST(test_set_model);
    RUN_TEST(test_interpret_single_not_initialized);
    RUN_TEST(test_interpret_single_null_params);
    RUN_TEST(test_interpret_multiple_empty);
    RUN_TEST(test_interpret_from_manager_no_dtcs);
    RUN_TEST(test_provider_string);
    RUN_TEST(test_default_groq_config);

    return UNITY_END();
}
