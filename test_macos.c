#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/time.h>

#include "core/types.h"
#include "core/error/error_handler.h"
#include "core/logger/logger.h"
#include "core/elm327/elm327.h"
#include "core/obd2/obd2.h"
#include "core/pid/pid_manager.h"
#include "core/dtc/dtc_manager.h"

static int serial_fd = -1;

static int serial_open(const char* port, int baudrate)
{
    serial_fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (serial_fd < 0) {
        printf("ERRO: Nao conseguiu abrir %s: %s\n", port, strerror(errno));
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(serial_fd, &tty) != 0) {
        printf("ERRO: tcgetattr falhou\n");
        return -1;
    }

    speed_t baud;
    switch (baudrate) {
        case 9600:   baud = B9600;   break;
        case 38400:  baud = B38400;  break;
        case 115200: baud = B115200; break;
        default:     baud = B38400;  break;
    }

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= (tcflag_t)~PARENB;
    tty.c_cflag &= (tcflag_t)~CSTOPB;
    tty.c_cflag &= (tcflag_t)~CSIZE;
    tty.c_cflag |= CS8;
#ifdef CRTSCTS
    tty.c_cflag &= (tcflag_t)~CRTSCTS;
#endif

    tty.c_lflag &= (tcflag_t)~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= (tcflag_t)~(IXON | IXOFF | IXANY);
    tty.c_iflag &= (tcflag_t)~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= (tcflag_t)~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;

    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        printf("ERRO: tcsetattr falhou\n");
        return -1;
    }

    tcflush(serial_fd, TCIOFLUSH);

    printf("Porta serial %s aberta com sucesso!\n", port);
    return 0;
}

static void serial_close(void)
{
    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }
}


static Result_t elm_write_cb(const u8* data, u16 length, void* context)
{
    UNUSED(context);

    if (serial_fd < 0) {
        return RESULT_ERROR;
    }

    ssize_t written = write(serial_fd, data, length);

    printf("TX: %.*s", (int)length, (const char*)data);

    return (written == (ssize_t)length) ? RESULT_OK : RESULT_ERROR;
}

static Result_t elm_read_cb(u8* data, u16 max_length, u16* actual_length, void* context)
{
    UNUSED(context);

    if (serial_fd < 0) {
        *actual_length = 0U;
        return RESULT_ERROR;
    }

    ssize_t n = read(serial_fd, data, max_length);

    if (n < 0) {
        *actual_length = 0U;
        return (errno == EAGAIN || errno == EWOULDBLOCK) ? RESULT_OK : RESULT_ERROR;
    }

    *actual_length = (u16)n;
    return RESULT_OK;
}

static u32 get_timestamp_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u32)((tv.tv_sec * 1000U) + (tv.tv_usec / 1000U));
}

static void error_callback(const ErrorInfo_t* error)
{
    printf("[%s] %s (at %s:%u)\n",
           ErrorHandler_GetSeverityString(error->severity),
           ErrorHandler_GetCodeString(error->code),
           error->file,
           error->line);
}

static ErrorHandler_t g_error_handler;
static Logger_t g_logger;
static Elm327_t g_elm;
static Obd2_t g_obd;
static PidManager_t g_pid_mgr;
static DtcManager_t g_dtc_mgr;


static u8 g_resp_buffer[256];
static u16 g_resp_length;

static Result_t send_and_wait(const char* cmd, int timeout_ms)
{
    Result_t result = Elm327_SendCommand(&g_elm, cmd);
    if (result != RESULT_OK) {
        return result;
    }

    int elapsed = 0;
    while (elapsed < timeout_ms) {
        Elm327_Update(&g_elm);

        if (Elm327_GetState(&g_elm) == ELM_STATE_IDLE) {
            break;
        }

        usleep(10000);
        elapsed += 10;
    }

    if (Elm327_GetState(&g_elm) != ELM_STATE_IDLE) {
        g_elm.state = ELM_STATE_IDLE;
        return RESULT_TIMEOUT;
    }

    g_resp_length = 0U;
    return Elm327_GetResponse(&g_elm, g_resp_buffer, sizeof(g_resp_buffer) - 1U, &g_resp_length);
}


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


static void print_menu(void)
{
    printf("\n");
    printf("========================================\n");
    printf("       OBD2 Scanner - MacOS            \n");
    printf("========================================\n");
    printf("\n");
    printf("  1. Ler dados em tempo real\n");
    printf("  2. Ler codigos de erro (DTC)\n");
    printf("  3. Limpar codigos de erro\n");
    printf("  4. Ler VIN\n");
    printf("  5. Enviar comando manual\n");
    printf("  6. Re-inicializar ELM327\n");
    printf("  0. Sair\n");
    printf("\n");
    printf("Escolha: ");
}


int main(int argc, char* argv[])
{
    const char* port = "/dev/tty.OBDLinkMX";

    if (argc > 1) {
        port = argv[1];
    }

    printf("\n");
    printf("=========================================\n");
    printf("  OBD2 Scanner para MacOS               \n");
    printf("  Porta: %s\n", port);
    printf("=========================================\n");
    printf("\n");
    printf("Uso: %s [porta_serial]\n", argv[0]);
    printf("Exemplo: %s /dev/tty.OBDLinkMX\n", argv[0]);
    printf("         %s /dev/tty.OBDII-SPP\n", argv[0]);
    printf("\n");

    if (serial_open(port, 38400) < 0) {
        printf("\nDica: Liste portas disponiveis com:\n");
        printf("  ls /dev/tty.*\n\n");
        return 1;
    }

    ErrorHandler_Init(&g_error_handler, error_callback);

    LoggerConfig_t log_cfg = {
        .min_level = LOG_LEVEL_INFO,
        .get_timestamp_ms = get_timestamp_ms
    };
    Logger_Init(&g_logger, &log_cfg);

    ElmConfig_t elm_cfg = {
        .write_callback = elm_write_cb,
        .read_callback = elm_read_cb,
        .callback_context = NULL,
        .get_timestamp_ms = get_timestamp_ms,
        .error_handler = &g_error_handler,
        .logger = &g_logger
    };
    Elm327_Init(&g_elm, &elm_cfg);

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
            default:
                printf("Opcao invalida.\n");
                break;
        }
    }

    serial_close();
    printf("\nAte mais!\n\n");

    return 0;
}
