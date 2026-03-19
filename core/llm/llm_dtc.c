#include "llm_dtc.h"
#include "../str_utils/str_utils.h"
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

static const char* const provider_strings[] = {
    [LLM_PROVIDER_GROQ] = "Groq"
};

static const char GROQ_DEFAULT_URL[] = "https://api.groq.com/openai/v1/chat/completions";
static const char GROQ_DEFAULT_MODEL[] = "llama-3.3-70b-versatile";

typedef struct {
    char* buffer;
    u16 size;
    u16 capacity;
} CurlWriteCtx_t;

static size_t llm_curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp)
{
    CurlWriteCtx_t* ctx = (CurlWriteCtx_t*)userp;
    size_t total = size * nmemb;

    if ((ctx->size + (u16)total) >= ctx->capacity) {
        total = (size_t)(ctx->capacity - ctx->size - 1U);
    }

    memcpy(ctx->buffer + ctx->size, contents, total);
    ctx->size += (u16)total;
    ctx->buffer[ctx->size] = '\0';

    return size * nmemb;
}

static u16 build_prompt_single(const char* dtc_code, char* buffer, u16 max_len)
{
    int n = snprintf(buffer, max_len,
        "You are an automotive diagnostic expert. "
        "Interpret the following OBD-II DTC code and respond in this exact format:\n"
        "DESCRIPTION: <one-line description>\n"
        "CAUSES: <common causes, comma-separated>\n"
        "SEVERITY: <LOW|MEDIUM|HIGH|CRITICAL>\n\n"
        "DTC code: %s",
        dtc_code);

    return (n > 0 && n < (int)max_len) ? (u16)n : 0U;
}

static u16 build_prompt_multiple(const Dtc_t* dtcs, u8 count, char* buffer, u16 max_len)
{
    int offset = snprintf(buffer, max_len,
        "You are an automotive diagnostic expert. "
        "Interpret the following OBD-II DTC codes. "
        "For EACH code, respond in this exact format:\n"
        "[CODE] <dtc_code>\n"
        "DESCRIPTION: <one-line description>\n"
        "CAUSES: <common causes, comma-separated>\n"
        "SEVERITY: <LOW|MEDIUM|HIGH|CRITICAL>\n\n"
        "DTC codes:\n");

    if (offset < 0 || offset >= (int)max_len) {
        return 0U;
    }

    for (u8 i = 0U; i < count; i++) {
        if (dtcs[i].valid == true) {
            int n = snprintf(buffer + offset, (size_t)(max_len - (u16)offset),
                             "- %s\n", dtcs[i].code_string);
            if (n > 0) {
                offset += n;
            }
        }
    }

    return (u16)offset;
}

static const char* find_field(const char* text, const char* field, char* out, u16 out_max)
{
    const char* pos = strstr(text, field);
    if (pos == NULL) {
        out[0] = '\0';
        return NULL;
    }

    pos += strlen(field);

    while (*pos == ' ' || *pos == ':') {
        pos++;
    }

    u16 i = 0U;
    while (pos[i] != '\0' && pos[i] != '\n' && i < (out_max - 1U)) {
        out[i] = pos[i];
        i++;
    }
    out[i] = '\0';

    return pos + i;
}

static void parse_single_result(const char* text, const char* dtc_code, LlmDtcResult_t* result)
{
    str_copy_safe(result->code, dtc_code, DTC_CODE_STRING_LEN);

    find_field(text, "DESCRIPTION:", result->description, sizeof(result->description));
    find_field(text, "CAUSES:", result->causes, sizeof(result->causes));
    find_field(text, "SEVERITY:", result->severity, sizeof(result->severity));

    result->valid = (result->description[0] != '\0');
}

static void parse_multiple_results(const char* text, const Dtc_t* dtcs, u8 count, LlmResponse_t* response)
{
    response->count = 0U;

    for (u8 i = 0U; i < count && i < LLM_DTC_INPUT_MAX; i++) {
        if (dtcs[i].valid == false) {
            continue;
        }

        char search_tag[16];
        snprintf(search_tag, sizeof(search_tag), "[%s]", dtcs[i].code_string);

        const char* block = strstr(text, search_tag);
        if (block == NULL) {
            char alt_tag[16];
            snprintf(alt_tag, sizeof(alt_tag), "%s", dtcs[i].code_string);
            block = strstr(text, alt_tag);
        }

        if (block != NULL) {
            parse_single_result(block, dtcs[i].code_string, &response->results[response->count]);
            response->count++;
        }
    }
}

static Result_t extract_content_from_json(const char* json, char* content, u16 max_len)
{
    const char* marker = "\"content\"";
    const char* pos = strstr(json, marker);
    if (pos == NULL) {
        content[0] = '\0';
        return RESULT_ERROR;
    }

    pos += strlen(marker);

    while (*pos != '\0' && *pos != '"') {
        pos++;
    }
    if (*pos == '"') {
        pos++;
    }

    u16 i = 0U;
    while (pos[i] != '\0' && i < (max_len - 1U)) {
        if (pos[i] == '"' && (i == 0U || pos[i - 1U] != '\\')) {
            break;
        }

        if (pos[i] == '\\' && pos[i + 1U] == 'n') {
            content[i] = '\n';
            i++;
            pos++;
            continue;
        }

        if (pos[i] == '\\' && pos[i + 1U] == '"') {
            content[i] = '"';
            i++;
            pos++;
            continue;
        }

        content[i] = pos[i];
        i++;
    }
    content[i] = '\0';

    return RESULT_OK;
}

static Result_t call_groq_api(LlmDtcInterpreter_t* llm, const char* prompt, char* response_buf, u16 response_max)
{
    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        return RESULT_ERROR;
    }

    char post_body[LLM_PROMPT_MAX + 512];
    char escaped_prompt[LLM_PROMPT_MAX];

    u16 ep = 0U;
    for (u16 i = 0U; prompt[i] != '\0' && ep < (LLM_PROMPT_MAX - 2U); i++) {
        if (prompt[i] == '"') {
            escaped_prompt[ep++] = '\\';
            escaped_prompt[ep++] = '"';
        } else if (prompt[i] == '\n') {
            escaped_prompt[ep++] = '\\';
            escaped_prompt[ep++] = 'n';
        } else if (prompt[i] == '\\') {
            escaped_prompt[ep++] = '\\';
            escaped_prompt[ep++] = '\\';
        } else {
            escaped_prompt[ep++] = prompt[i];
        }
    }
    escaped_prompt[ep] = '\0';

    snprintf(post_body, sizeof(post_body),
        "{\"model\":\"%s\","
        "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
        "\"temperature\":%.1f,"
        "\"max_tokens\":%u}",
        llm->config.model,
        escaped_prompt,
        (double)llm->config.temperature,
        (unsigned int)llm->config.max_tokens);

    char auth_header[LLM_API_KEY_MAX + 32];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", llm->config.api_key);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    CurlWriteCtx_t write_ctx;
    write_ctx.buffer = response_buf;
    write_ctx.size = 0U;
    write_ctx.capacity = response_max;
    response_buf[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, llm->config.base_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, llm_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)llm->config.timeout_seconds);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    llm->total_requests++;

    if (res != CURLE_OK) {
        llm->total_errors++;
        return (res == CURLE_OPERATION_TIMEDOUT) ? RESULT_TIMEOUT : RESULT_COMM_ERROR;
    }

    return RESULT_OK;
}

Result_t LlmDtc_Init(LlmDtcInterpreter_t* llm, const LlmConfig_t* config)
{
    if (llm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    if (config == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    if (config->api_key[0] == '\0') {
        return RESULT_INVALID_PARAM;
    }

    llm->config = *config;
    llm->total_requests = 0U;
    llm->total_errors = 0U;
    llm->initialized = true;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    return RESULT_OK;
}

Result_t LlmDtc_InitGroq(LlmDtcInterpreter_t* llm, const char* api_key)
{
    if (llm == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    if (api_key == NULL_PTR || api_key[0] == '\0') {
        return RESULT_INVALID_PARAM;
    }

    LlmConfig_t config;
    str_copy_safe(config.api_key, api_key, LLM_API_KEY_MAX);
    str_copy_safe(config.model, GROQ_DEFAULT_MODEL, LLM_MODEL_MAX);
    str_copy_safe(config.base_url, GROQ_DEFAULT_URL, LLM_URL_MAX);
    config.provider = LLM_PROVIDER_GROQ;
    config.timeout_seconds = 30U;
    config.temperature = 0.2f;
    config.max_tokens = 1024U;

    return LlmDtc_Init(llm, &config);
}

Result_t LlmDtc_InterpretSingle(LlmDtcInterpreter_t* llm,
                                 const char* dtc_code,
                                 LlmDtcResult_t* result)
{
    if (llm == NULL_PTR || dtc_code == NULL_PTR || result == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    if (llm->initialized == false) {
        return RESULT_NOT_READY;
    }

    result->valid = false;

    char prompt[LLM_PROMPT_MAX];
    u16 prompt_len = build_prompt_single(dtc_code, prompt, sizeof(prompt));
    if (prompt_len == 0U) {
        return RESULT_ERROR;
    }

    char raw_json[LLM_RESPONSE_MAX];
    Result_t r = call_groq_api(llm, prompt, raw_json, sizeof(raw_json));
    if (r != RESULT_OK) {
        return r;
    }

    char content[LLM_RESPONSE_MAX];
    r = extract_content_from_json(raw_json, content, sizeof(content));
    if (r != RESULT_OK) {
        return RESULT_ERROR;
    }

    parse_single_result(content, dtc_code, result);

    return result->valid ? RESULT_OK : RESULT_ERROR;
}

Result_t LlmDtc_InterpretMultiple(LlmDtcInterpreter_t* llm,
                                   const Dtc_t* dtcs,
                                   u8 count,
                                   LlmResponse_t* response)
{
    if (llm == NULL_PTR || dtcs == NULL_PTR || response == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    if (llm->initialized == false) {
        return RESULT_NOT_READY;
    }

    if (count == 0U) {
        response->count = 0U;
        response->success = false;
        return RESULT_NO_DATA;
    }

    response->success = false;
    response->count = 0U;

    char prompt[LLM_PROMPT_MAX];
    u16 prompt_len = build_prompt_multiple(dtcs, count, prompt, sizeof(prompt));
    if (prompt_len == 0U) {
        return RESULT_ERROR;
    }

    Result_t r = call_groq_api(llm, prompt, response->raw_response, LLM_RESPONSE_MAX);
    if (r != RESULT_OK) {
        return r;
    }

    response->raw_length = (u16)strlen(response->raw_response);

    char content[LLM_RESPONSE_MAX];
    r = extract_content_from_json(response->raw_response, content, sizeof(content));
    if (r != RESULT_OK) {
        return RESULT_ERROR;
    }

    parse_multiple_results(content, dtcs, count, response);
    response->success = (response->count > 0U);

    return response->success ? RESULT_OK : RESULT_ERROR;
}

Result_t LlmDtc_InterpretFromManager(LlmDtcInterpreter_t* llm,
                                      const DtcManager_t* dm,
                                      DtcType_t type,
                                      LlmResponse_t* response)
{
    if (llm == NULL_PTR || dm == NULL_PTR || response == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    Dtc_t dtcs[LLM_DTC_INPUT_MAX];
    u8 actual_count = 0U;

    Result_t r = DtcManager_GetAllDtcs(dm, type, dtcs, LLM_DTC_INPUT_MAX, &actual_count);
    if (r != RESULT_OK) {
        return r;
    }

    if (actual_count == 0U) {
        response->count = 0U;
        response->success = false;
        return RESULT_NO_DATA;
    }

    return LlmDtc_InterpretMultiple(llm, dtcs, actual_count, response);
}

Result_t LlmDtc_SetApiKey(LlmDtcInterpreter_t* llm, const char* api_key)
{
    if (llm == NULL_PTR || api_key == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    str_copy_safe(llm->config.api_key, api_key, LLM_API_KEY_MAX);

    return RESULT_OK;
}

Result_t LlmDtc_SetModel(LlmDtcInterpreter_t* llm, const char* model)
{
    if (llm == NULL_PTR || model == NULL_PTR) {
        return RESULT_INVALID_PARAM;
    }

    str_copy_safe(llm->config.model, model, LLM_MODEL_MAX);

    return RESULT_OK;
}

const char* LlmDtc_GetProviderString(LlmProvider_t provider)
{
    if (provider >= LLM_PROVIDER_MAX) {
        return "Unknown";
    }

    return provider_strings[provider];
}
