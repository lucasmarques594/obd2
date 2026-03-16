#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

#include "core/types.h"
#include "core/error/error_handler.h"
#include "core/elm327/elm327.h"
#include "core/obd2/obd2.h"
#include "core/pid/pid_manager.h"
#include "core/dtc/dtc_manager.h"

static int serial_fd = -1;

int serial_open(const char* port, int baudrate)
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
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    
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

void serial_close(void)
{
    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }
}

int serial_write(const char* data)
{
    if (serial_fd < 0) return -1;
    
    size_t len = strlen(data);
    ssize_t written = write(serial_fd, data, len);
    
    printf("TX: %s", data);
    
    return (written == (ssize_t)len) ? 0 : -1;
}

int serial_read(char* buffer, size_t max_len, int timeout_ms)
{
    if (serial_fd < 0) return -1;
    
    size_t total = 0;
    int elapsed = 0;
    
    while (elapsed < timeout_ms && total < max_len - 1) {
        ssize_t n = read(serial_fd, buffer + total, max_len - total - 1);
        
        if (n > 0) {
            total += n;
            if (strchr(buffer, '>') != NULL) {
                break;
            }
        }
        
        usleep(10000);
        elapsed += 10;
    }
    
    buffer[total] = '\0';
    
    if (total > 0) {
        printf("RX: %s\n", buffer);
    }
    
    return (int)total;
}

char* send_command(const char* cmd)
{
    static char response[256];
    char cmd_with_cr[64];
    
    snprintf(cmd_with_cr, sizeof(cmd_with_cr), "%s\r", cmd);
    
    serial_write(cmd_with_cr);
    usleep(100000);
    serial_read(response, sizeof(response), 3000);
    
    return response;
}

void init_elm327(void)
{
    printf("\n=== Inicializando ELM327 ===\n\n");
    
    send_command("ATZ");
    usleep(1000000);
    
    send_command("ATE0");
    usleep(100000);
    
    send_command("ATL0");
    usleep(100000);
    
    send_command("ATS0");
    usleep(100000);
    
    send_command("ATH0");
    usleep(100000);
    
    send_command("ATSP0");
    usleep(500000);
    
    printf("\n=== ELM327 Inicializado ===\n\n");
}

float parse_rpm(const char* response)
{
    unsigned int a, b;
    if (sscanf(response, "410C%02X%02X", &a, &b) == 2) {
        return ((a * 256.0f) + b) / 4.0f;
    }
    return -1;
}

float parse_speed(const char* response)
{
    unsigned int a;
    if (sscanf(response, "410D%02X", &a) == 1) {
        return (float)a;
    }
    return -1;
}

float parse_coolant_temp(const char* response)
{
    unsigned int a;
    if (sscanf(response, "4105%02X", &a) == 1) {
        return (float)a - 40.0f;
    }
    return -1;
}

float parse_engine_load(const char* response)
{
    unsigned int a;
    if (sscanf(response, "4104%02X", &a) == 1) {
        return (a * 100.0f) / 255.0f;
    }
    return -1;
}

float parse_throttle(const char* response)
{
    unsigned int a;
    if (sscanf(response, "4111%02X", &a) == 1) {
        return (a * 100.0f) / 255.0f;
    }
    return -1;
}

void read_live_data(void)
{
    char* resp;
    float value;
    
    printf("\n=== Dados em Tempo Real ===\n\n");
    
    resp = send_command("010C");
    value = parse_rpm(resp);
    if (value >= 0) printf("RPM:           %.0f\n", value);
    
    resp = send_command("010D");
    value = parse_speed(resp);
    if (value >= 0) printf("Velocidade:    %.0f km/h\n", value);
    
    resp = send_command("0105");
    value = parse_coolant_temp(resp);
    if (value >= -40) printf("Temp. Motor:   %.0f °C\n", value);
    
    resp = send_command("0104");
    value = parse_engine_load(resp);
    if (value >= 0) printf("Carga Motor:   %.1f %%\n", value);
    
    resp = send_command("0111");
    value = parse_throttle(resp);
    if (value >= 0) printf("Acelerador:    %.1f %%\n", value);
    
    printf("\n");
}

void read_dtcs(void)
{
    printf("\n=== Codigos de Erro (DTC) ===\n\n");
    
    char* resp = send_command("03");
    
    if (strstr(resp, "NODATA") || strstr(resp, "NO DATA")) {
        printf("Nenhum codigo de erro encontrado.\n\n");
        return;
    }
    
    printf("Resposta bruta: %s\n", resp);
    printf("\n");
}

void read_vin(void)
{
    printf("\n=== VIN do Veiculo ===\n\n");
    
    char* resp = send_command("0902");
    
    if (strstr(resp, "NODATA") || strstr(resp, "NO DATA")) {
        printf("VIN nao disponivel.\n\n");
        return;
    }
    
    printf("Resposta bruta: %s\n", resp);
    printf("\n");
}

void print_menu(void)
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
    printf("  Porta: %s                             \n", port);
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
    
    init_elm327();
    
    int running = 1;
    while (running) {
        print_menu();
        
        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
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
                printf("\nLimpando DTCs...\n");
                send_command("04");
                printf("DTCs limpos!\n");
                break;
                
            case 4:
                read_vin();
                break;
                
            case 5: {
                char cmd[64];
                printf("Digite o comando: ");
                scanf("%63s", cmd);
                send_command(cmd);
                break;
            }
                
            case 6:
                init_elm327();
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
