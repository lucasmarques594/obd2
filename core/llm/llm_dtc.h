#ifndef LLM_DTC_H
#define LLM_DTC_H

#include "../types.h"
#include "../dtc/dtc_manager.h"

#define LLM_API_KEY_MAX 128
#define LLM_MODEL_MAX 64
#define LLM_URL_MAX 256
#define LLM_RESPONSE_MAX 4096
#define LLM_PROMPT_MAX 1024
#define LLM_DTC_INPUT_MAX 16

typedef enum {
    LLM_PROVIDER_GROQ = 0,
    LLM_PROVIDER_MAX
} LlmProvider_t;

typedef struct {
    char api_key[LLM_API_KEY_MAX];
    char model[LLM_MODEL_MAX];
    char base_url[LLM_URL_MAX];
    LlmProvider_t provider;
    u32 timeout_seconds;
    float temperature;
    u16 max_tokens;
} LlmConfig_t;

typedef struct {
    char code[DTC_CODE_STRING_LEN];
    char description[256];
    char causes[512];
    char severity[32];
    bool valid;
} LlmDtcResult_t;

typedef struct {
    LlmDtcResult_t results[LLM_DTC_INPUT_MAX];
    u8 count;
    char raw_response[LLM_RESPONSE_MAX];
    u16 raw_length;
    bool success;
} LlmResponse_t;

typedef struct {
    LlmConfig_t config;
    bool initialized;
    u32 total_requests;
    u32 total_errors;
} LlmDtcInterpreter_t;

Result_t LlmDtc_Init(LlmDtcInterpreter_t* llm, const LlmConfig_t* config);

Result_t LlmDtc_InitGroq(LlmDtcInterpreter_t* llm, const char* api_key);

Result_t LlmDtc_InterpretSingle(LlmDtcInterpreter_t* llm,
                                 const char* dtc_code,
                                 LlmDtcResult_t* result);

Result_t LlmDtc_InterpretMultiple(LlmDtcInterpreter_t* llm,
                                   const Dtc_t* dtcs,
                                   u8 count,
                                   LlmResponse_t* response);

Result_t LlmDtc_InterpretFromManager(LlmDtcInterpreter_t* llm,
                                      const DtcManager_t* dm,
                                      DtcType_t type,
                                      LlmResponse_t* response);

Result_t LlmDtc_SetApiKey(LlmDtcInterpreter_t* llm, const char* api_key);

Result_t LlmDtc_SetModel(LlmDtcInterpreter_t* llm, const char* model);

const char* LlmDtc_GetProviderString(LlmProvider_t provider);

#endif
