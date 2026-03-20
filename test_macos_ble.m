#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "core/types.h"
#include "core/error/error_handler.h"
#include "core/logger/logger.h"
#include "core/elm327/elm327.h"
#include "core/obd2/obd2.h"
#include "core/pid/pid_manager.h"
#include "core/dtc/dtc_manager.h"
#include "core/llm/llm_dtc.h"
#include "ble_bridge/ble_elm327.h"

/* ---------------------------------------------------------------------------
 * Timestamp
 * ---------------------------------------------------------------------------*/

static u32 get_timestamp_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u32)((tv.tv_sec * 1000U) + (tv.tv_usec / 1000U));
}

/* ---------------------------------------------------------------------------
 * Error callback
 * ---------------------------------------------------------------------------*/

static void error_callback(const ErrorInfo_t* error)
{
    printf("[%s] %s (at %s:%u)\n",
           ErrorHandler_GetSeverityString(error->severity),
           ErrorHandler_GetCodeString(error->code),
           error->file,
           error->line);
}

/* ---------------------------------------------------------------------------
 * Core instances
 * ---------------------------------------------------------------------------*/

static ErrorHandler_t g_error_handler;
static Logger_t g_logger;
static Elm327_t g_elm;
static Obd2_t g_obd;
static PidManager_t g_pid_mgr;
static DtcManager_t g_dtc_mgr;
static LlmDtcInterpreter_t g_llm;
static bool g_llm_available = false;
static BleElm327_t* g_ble = NULL;

/* ---------------------------------------------------------------------------
 * Blocking send/receive via BLE
 * ---------------------------------------------------------------------------*/

static u8 g_resp_buffer[256];
static u16 g_resp_length;

static Result_t send_and_wait(const char* cmd, u32 timeout_ms)
{
    g_resp_length = 0U;
    return BleElm327_SendCommand(g_ble, cmd,
                                  (char*)g_resp_buffer, sizeof(g_resp_buffer) - 1U,
                                  &g_resp_length, timeout_ms);
}

/* ---------------------------------------------------------------------------
 * Init ELM327
 * ---------------------------------------------------------------------------*/

static void init_elm327_sequence(void)
{
    printf("\n=== Inicializando ELM327 ===\n\n");

    const char* init_cmds[] = {
        "ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0"
    };
    const int init_delays[] = {
        1000000, 100000, 100000, 100000, 100000, 500000
    };

    for (u8 i = 0U; i < 6U; i++) {
        Result_t r = send_and_wait(init_cmds[i], 5000);

        if (r == RESULT_OK && g_resp_length > 0U) {
            printf("  %s -> %.*s\n", init_cmds[i], (int)g_resp_length, (const char*)g_resp_buffer);
        } else {
            printf("  %s -> (sem resposta)\n", init_cmds[i]);
        }

        usleep((unsigned int)init_delays[i]);
    }

    printf("\n=== ELM327 Inicializado ===\n\n");
}

/* ---------------------------------------------------------------------------
 * Read live data
 * ---------------------------------------------------------------------------*/

static const u8 live_pids[] = { 0x0CU, 0x0DU, 0x05U, 0x04U, 0x11U };
static const char* live_cmds[] = { "010C", "010D", "0105", "0104", "0111" };
#define LIVE_PID_COUNT 5U

static void read_live_data(void)
{
    printf("\n=== Dados em Tempo Real ===\n\n");

    for (u8 i = 0U; i < LIVE_PID_COUNT; i++) {
        Result_t r = send_and_wait(live_cmds[i], 3000);

        if (r != RESULT_OK || g_resp_length == 0U) {
            continue;
        }

        ElmResponse_t elm_resp = Elm327_ParseResponse(g_resp_buffer, g_resp_length);
        if (elm_resp == ELM_RESP_NO_DATA || elm_resp == ELM_RESP_ERROR) {
            continue;
        }

        Obd2Frame_t frame;
        g_obd.pending_request.mode = 0x01U;
        g_obd.pending_request.pid = live_pids[i];
        g_obd.request_pending = true;

        r = Obd2_ProcessResponse(&g_obd, g_resp_buffer, g_resp_length, &frame);
        if (r != RESULT_OK || frame.valid == false) {
            continue;
        }

        PidValue_t value;
        r = PidManager_ConvertRawToEng(live_pids[i], frame.data, frame.data_length, &value);
        if (r != RESULT_OK || value.valid == false) {
            continue;
        }

        const PidDefinition_t* def = PidManager_GetDefinition(live_pids[i]);
        const char* unit = PidManager_GetUnitString(value.unit);
        const char* name = (def != NULL) ? def->name : live_cmds[i];

        printf("  %-25s %8.1f %s\n", name, (double)value.eng_value, unit);
    }

    printf("\n");
}

/* ---------------------------------------------------------------------------
 * Read DTCs
 * ---------------------------------------------------------------------------*/

static void read_dtcs(void)
{
    printf("\n=== Codigos de Erro (DTC) ===\n\n");

    Result_t r = send_and_wait("03", 5000);

    if (r != RESULT_OK || g_resp_length == 0U) {
        printf("  Sem resposta da ECU.\n\n");
        return;
    }

    ElmResponse_t elm_resp = Elm327_ParseResponse(g_resp_buffer, g_resp_length);
    if (elm_resp == ELM_RESP_NO_DATA) {
        printf("  Nenhum codigo de erro encontrado.\n\n");
        return;
    }

    Obd2Frame_t frame;
    g_obd.pending_request.mode = 0x03U;
    g_obd.pending_request.pid = 0x00U;
    g_obd.request_pending = true;

    r = Obd2_ProcessResponse(&g_obd, g_resp_buffer, g_resp_length, &frame);
    if (r != RESULT_OK) {
        printf("  Resposta bruta: %.*s\n\n", (int)g_resp_length, (const char*)g_resp_buffer);
        return;
    }

    DtcManager_ProcessResponse(&g_dtc_mgr, frame.data, frame.data_length, DTC_TYPE_CURRENT);

    u8 count = DtcManager_GetCount(&g_dtc_mgr, DTC_TYPE_CURRENT);

    if (count == 0U) {
        printf("  Nenhum codigo de erro encontrado.\n\n");
        return;
    }

    printf("  %u codigo(s) encontrado(s):\n\n", count);

    for (u8 i = 0U; i < count; i++) {
        Dtc_t dtc;
        if (DtcManager_GetDtc(&g_dtc_mgr, DTC_TYPE_CURRENT, i, &dtc) == RESULT_OK) {
            printf("    [%u] %s  (%s)\n",
                   (unsigned int)(i + 1U),
                   dtc.code_string,
                   DtcManager_GetSystemString(dtc.system));
        }
    }

    printf("\n");
}

/* ---------------------------------------------------------------------------
 * Clear DTCs
 * ---------------------------------------------------------------------------*/

static void clear_dtcs(void)
{
    printf("\n  Limpando DTCs...\n");

    Result_t r = send_and_wait("04", 5000);

    if (r == RESULT_OK) {
        DtcManager_Clear(&g_dtc_mgr);
        printf("  DTCs limpos!\n\n");
    } else {
        printf("  Falha ao limpar DTCs.\n\n");
    }
}

/* ---------------------------------------------------------------------------
 * Read VIN
 * ---------------------------------------------------------------------------*/

static void read_vin(void)
{
    printf("\n=== VIN do Veiculo ===\n\n");

    Result_t r = send_and_wait("0902", 5000);

    if (r != RESULT_OK || g_resp_length == 0U) {
        printf("  VIN nao disponivel.\n\n");
        return;
    }

    ElmResponse_t elm_resp = Elm327_ParseResponse(g_resp_buffer, g_resp_length);
    if (elm_resp == ELM_RESP_NO_DATA) {
        printf("  VIN nao disponivel.\n\n");
        return;
    }

    printf("  Resposta: %.*s\n\n", (int)g_resp_length, (const char*)g_resp_buffer);
}

/* ---------------------------------------------------------------------------
 * Manual command
 * ---------------------------------------------------------------------------*/

static void manual_command(void)
{
    char cmd[64];
    printf("  Digite o comando: ");
    if (scanf("%63s", cmd) < 1) {
        printf("  Entrada invalida.\n\n");
        return;
    }

    Result_t r = send_and_wait(cmd, 5000);

    if (r == RESULT_OK && g_resp_length > 0U) {
        printf("  Resposta: %.*s\n\n", (int)g_resp_length, (const char*)g_resp_buffer);
    } else {
        printf("  Sem resposta.\n\n");
    }
}

/* ---------------------------------------------------------------------------
 * Interpret DTCs with LLM
 * ---------------------------------------------------------------------------*/

static void interpret_dtcs_llm(void)
{
    if (g_llm_available == false) {
        printf("\n  LLM nao configurado. Defina GROQ_API_KEY no ambiente.\n\n");
        return;
    }

    u8 count = DtcManager_GetCount(&g_dtc_mgr, DTC_TYPE_CURRENT);

    if (count == 0U) {
        printf("\n  Nenhum DTC em memoria. Leia os DTCs primeiro (opcao 2).\n\n");
        return;
    }

    printf("\n=== Interpretacao LLM (%s) ===\n\n", g_llm.config.model);
    printf("  Consultando %u DTC(s) via Groq...\n\n", count);

    LlmResponse_t response;
    Result_t r = LlmDtc_InterpretFromManager(&g_llm, &g_dtc_mgr, DTC_TYPE_CURRENT, &response);

    if (r != RESULT_OK || response.success == false) {
        printf("  Falha na consulta ao LLM.\n\n");
        return;
    }

    for (u8 i = 0U; i < response.count; i++) {
        LlmDtcResult_t* res = &response.results[i];
        if (res->valid == false) {
            continue;
        }

        printf("  --- %s ---\n", res->code);
        printf("  Descricao:  %s\n", res->description);
        printf("  Causas:     %s\n", res->causes);
        printf("  Severidade: %s\n", res->severity);
        printf("\n");
    }
}

/* ---------------------------------------------------------------------------
 * Menu
 * ---------------------------------------------------------------------------*/

static void print_menu(void)
{
    printf("\n");
    printf("========================================\n");
    printf("       OBD2 Scanner BLE - MacOS        \n");
    printf("========================================\n");
    printf("\n");
    printf("  1. Ler dados em tempo real\n");
    printf("  2. Ler codigos de erro (DTC)\n");
    printf("  3. Limpar codigos de erro\n");
    printf("  4. Ler VIN\n");
    printf("  5. Enviar comando manual\n");
    printf("  6. Re-inicializar ELM327\n");
    printf("  7. Interpretar DTCs com IA%s\n", g_llm_available ? "" : " [indisponivel]");
    printf("  0. Sair\n");
    printf("\n");
    printf("Escolha: ");
}

/* ---------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------------*/

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    printf("\n");
    printf("=========================================\n");
    printf("  OBD2 Scanner BLE para MacOS           \n");
    printf("=========================================\n");
    printf("\n");

    @autoreleasepool {

    /* Init core modules */
    ErrorHandler_Init(&g_error_handler, error_callback);

    LoggerConfig_t log_cfg = {
        .min_level = LOG_LEVEL_INFO,
        .get_timestamp_ms = get_timestamp_ms
    };
    Logger_Init(&g_logger, &log_cfg);

    Obd2Config_t obd_cfg = {
        .elm = &g_elm,
        .error_handler = &g_error_handler,
        .response_callback = NULL,
        .callback_context = NULL
    };
    Obd2_Init(&g_obd, &obd_cfg);

    PidManagerConfig_t pid_cfg = {
        .error_handler = &g_error_handler,
        .value_callback = NULL,
        .callback_context = NULL,
        .get_timestamp_ms = get_timestamp_ms
    };
    PidManager_Init(&g_pid_mgr, &pid_cfg);

    DtcManagerConfig_t dtc_cfg = {
        .error_handler = &g_error_handler,
        .dtc_callback = NULL,
        .cleared_callback = NULL,
        .callback_context = NULL,
        .get_timestamp_ms = get_timestamp_ms
    };
    DtcManager_Init(&g_dtc_mgr, &dtc_cfg);

    /* Init LLM */
    const char* groq_key = getenv("GROQ_API_KEY");
    if (groq_key != NULL && groq_key[0] != '\0') {
        if (LlmDtc_InitGroq(&g_llm, groq_key) == RESULT_OK) {
            g_llm_available = true;
            printf("LLM configurado: Groq (%s)\n\n", g_llm.config.model);
        }
    } else {
        printf("GROQ_API_KEY nao definida — interpretacao IA indisponivel.\n\n");
    }

    /* Init BLE */
    g_ble = BleElm327_Create();
    if (g_ble == NULL) {
        printf("ERRO: falha ao criar BLE bridge.\n");
        return 1;
    }

    printf("=== Conectando via BLE ===\n\n");

    BleElm327_InitDefault(g_ble);

    Result_t r = BleElm327_ScanAndConnect(g_ble);

    if (r != RESULT_OK) {
        printf("\nFalha ao conectar no ELM327 via BLE.\n");
        printf("Verifique:\n");
        printf("  - Bluetooth ligado no Mac\n");
        printf("  - Adaptador plugado na porta OBD2\n");
        printf("  - Ignicao na posicao ON\n\n");
        BleElm327_Destroy(g_ble);
        return 1;
    }

    printf("\nConectado em: %s\n", BleElm327_GetDeviceName(g_ble));

    init_elm327_sequence();

    int running = 1;
    while (running) {
        print_menu();

        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n') { /* flush */ }
            continue;
        }

        switch (choice) {
            case 0:
                running = 0;
                break;
            case 1:
                read_live_data();
                break;
            case 2:
                read_dtcs();
                break;
            case 3:
                clear_dtcs();
                break;
            case 4:
                read_vin();
                break;
            case 5:
                manual_command();
                break;
            case 6:
                init_elm327_sequence();
                break;
            case 7:
                interpret_dtcs_llm();
                break;
            default:
                printf("Opcao invalida.\n");
                break;
        }
    }

    BleElm327_Disconnect(g_ble);
    BleElm327_Destroy(g_ble);
    printf("\nAte mais!\n\n");

    } /* @autoreleasepool */

    return 0;
}
