#include "../inc/modbus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/time.h>

/**
 * @brief Inicializa a comunicação MODBUS serial
 */
int modbus_init(const char *porta) {
    int fd = open(porta, O_RDWR | O_NOCTTY | O_NDELAY);
    
    if (fd == -1) {
        perror("Erro ao abrir porta serial MODBUS");
        return -1;
    }
    
    struct termios options;
    tcgetattr(fd, &options);
    
    // Configura baud rate para 115200
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    
    // Configuração: 8N1, RTS/DE controlado por driver
    options.c_cflag &= ~PARENB;        // Sem paridade
    options.c_cflag &= ~CSTOPB;        // 1 stop bit
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;            // 8 bits de dados
    options.c_cflag |= CREAD | CLOCAL; // Habilita recepção
    
    // Modo raw
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    
    // Timeout de 100ms
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;
    
    tcsetattr(fd, TCSANOW, &options);
    tcflush(fd, TCIOFLUSH);
    
    printf("[MODBUS] Porta %s inicializada com sucesso (115200 8N1)\n", porta);
    return fd;
}

/**
 * @brief Fecha a comunicação MODBUS
 */
void modbus_close(int fd) {
    if (fd >= 0) {
        close(fd);
        printf("[MODBUS] Porta serial fechada\n");
    }
}

/**
 * @brief Calcula CRC16 MODBUS
 */
uint16_t modbus_crc16(uint8_t *buffer, int length) {
    uint16_t crc = 0xFFFF;
    
    for (int i = 0; i < length; i++) {
        crc ^= buffer[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief Lê holding registers (função 0x03)
 */
bool modbus_read_holding_registers(int fd, uint8_t slave_addr, uint16_t start_addr, 
                                   uint16_t num_regs, uint16_t *output) {
    uint8_t request[12];  // Aumentado para incluir matrícula (4 bytes) + CRC (2 bytes)
    uint8_t response[256];
    
    // Monta requisição MODBUS
    request[0] = slave_addr;
    request[1] = MODBUS_FUNC_READ_HOLDING;
    request[2] = (start_addr >> 8) & 0xFF;
    request[3] = start_addr & 0xFF;
    request[4] = (num_regs >> 8) & 0xFF;
    request[5] = num_regs & 0xFF;
    
    // ATENÇÃO: Adiciona os 4 últimos dígitos da matrícula (7700) antes do CRC
    // Conforme especificação: "É necessário enviar os 4 últimos dígitos da matrícula 
    // ao final de cada mensagem, sempre antes do CRC."
    request[6] = '7';  // ASCII 0x37
    request[7] = '7';  // ASCII 0x37
    request[8] = '0';  // ASCII 0x30
    request[9] = '0';  // ASCII 0x30
    
    // Calcula CRC incluindo a matrícula
    uint16_t crc = modbus_crc16(request, 10);
    request[10] = crc & 0xFF;
    request[11] = (crc >> 8) & 0xFF;
    
    // Envia requisição (agora com 12 bytes: 6 dados + 4 matrícula + 2 CRC)
    tcflush(fd, TCIOFLUSH);
    int written = write(fd, request, 12);
    if (written != 12) {
        fprintf(stderr, "[MODBUS] Erro ao enviar requisição: %s\n", strerror(errno));
        return false;
    }
    
    // Aguarda resposta com timeout
    usleep(MODBUS_TIMEOUT_MS * 1000);
    
    int expected_len = 5 + (num_regs * 2);
    int bytes_read = read(fd, response, expected_len);
    
    if (bytes_read < expected_len) {
        fprintf(stderr, "[MODBUS] Timeout ou resposta incompleta (esperado %d, recebido %d)\n", 
                expected_len, bytes_read);
        return false;
    }
    
    // Verifica endereço e função
    if (response[0] != slave_addr || response[1] != MODBUS_FUNC_READ_HOLDING) {
        fprintf(stderr, "[MODBUS] Resposta inválida (addr=0x%02X func=0x%02X)\n", 
                response[0], response[1]);
        return false;
    }
    
    // Verifica CRC
    uint16_t received_crc = response[bytes_read - 2] | (response[bytes_read - 1] << 8);
    uint16_t calculated_crc = modbus_crc16(response, bytes_read - 2);
    
    if (received_crc != calculated_crc) {
        fprintf(stderr, "[MODBUS] Erro de CRC (esperado 0x%04X, recebido 0x%04X)\n", 
                calculated_crc, received_crc);
        return false;
    }
    
    // Extrai dados (Big Endian)
    int byte_count = response[2];
    for (int i = 0; i < num_regs; i++) {
        output[i] = (response[3 + i * 2] << 8) | response[4 + i * 2];
    }
    
    return true;
}

/**
 * @brief Escreve múltiplos holding registers (função 0x10)
 */
bool modbus_write_multiple_registers(int fd, uint8_t slave_addr, uint16_t start_addr,
                                     uint16_t num_regs, uint16_t *data) {
    uint8_t request[260];  // Aumentado para incluir matrícula (4 bytes)
    uint8_t response[12];  // Aumentado para incluir matrícula na resposta
    
    int byte_count = num_regs * 2;
    
    // Monta requisição MODBUS
    request[0] = slave_addr;
    request[1] = MODBUS_FUNC_WRITE_MULTIPLE;
    request[2] = (start_addr >> 8) & 0xFF;
    request[3] = start_addr & 0xFF;
    request[4] = (num_regs >> 8) & 0xFF;
    request[5] = num_regs & 0xFF;
    request[6] = byte_count;
    
    // Dados (Big Endian)
    for (int i = 0; i < num_regs; i++) {
        request[7 + i * 2] = (data[i] >> 8) & 0xFF;
        request[8 + i * 2] = data[i] & 0xFF;
    }
    
    // ATENÇÃO: Adiciona os 4 últimos dígitos da matrícula (7700) antes do CRC
    // Conforme especificação: "É necessário enviar os 4 últimos dígitos da matrícula 
    // ao final de cada mensagem, sempre antes do CRC."
    int data_len = 7 + byte_count;
    request[data_len]     = '7';  // ASCII 0x37
    request[data_len + 1] = '7';  // ASCII 0x37
    request[data_len + 2] = '0';  // ASCII 0x30
    request[data_len + 3] = '0';  // ASCII 0x30
    
    // Calcula CRC incluindo a matrícula
    int total_len = data_len + 4;  // Dados + matrícula (4 bytes)
    uint16_t crc = modbus_crc16(request, total_len);
    request[total_len] = crc & 0xFF;
    request[total_len + 1] = (crc >> 8) & 0xFF;
    
    // Envia requisição (dados + 4 bytes matrícula + 2 bytes CRC)
    tcflush(fd, TCIOFLUSH);
    int written = write(fd, request, total_len + 2);
    if (written != total_len + 2) {
        fprintf(stderr, "[MODBUS] Erro ao enviar escrita múltipla\n");
        return false;
    }
    
    // Aguarda confirmação
    usleep(MODBUS_TIMEOUT_MS * 1000);
    int bytes_read = read(fd, response, 8);
    
    if (bytes_read < 8) {
        fprintf(stderr, "[MODBUS] Timeout na confirmação de escrita\n");
        return false;
    }
    
    // Verifica resposta
    if (response[0] != slave_addr || response[1] != MODBUS_FUNC_WRITE_MULTIPLE) {
        fprintf(stderr, "[MODBUS] Erro na confirmação de escrita\n");
        return false;
    }
    
    return true;
}

// ========== Funções específicas para LPR ==========

/**
 * @brief Dispara captura de placa na câmera LPR
 */
bool lpr_trigger_capture(int fd, uint8_t camera_addr) {
    uint16_t trigger_value = 1;
    
    bool success = modbus_write_multiple_registers(fd, camera_addr, 1, 1, &trigger_value);
    
    if (success) {
        printf("[LPR 0x%02X] Trigger disparado\n", camera_addr);
    } else {
        fprintf(stderr, "[LPR 0x%02X] Erro ao disparar trigger\n", camera_addr);
    }
    
    return success;
}

/**
 * @brief Lê dados da câmera LPR
 */
bool lpr_read_data(int fd, uint8_t camera_addr, LPRData *data) {
    uint16_t registers[8];
    
    // Lê 8 registros a partir do offset 0 (status, trigger, placa[4 regs], confiança, erro)
    if (!modbus_read_holding_registers(fd, camera_addr, 0, 8, registers)) {
        return false;
    }
    
    // Status (offset 0)
    data->status = (LPRStatus)(registers[0] & 0xFF);
    
    // Trigger (offset 1)
    data->trigger = (registers[1] & 0xFF) != 0;
    
    // Placa (offsets 2-5) - 8 caracteres ASCII em 4 registros (2 bytes cada)
    data->placa[0] = (registers[2] >> 8) & 0xFF;
    data->placa[1] = registers[2] & 0xFF;
    data->placa[2] = (registers[3] >> 8) & 0xFF;
    data->placa[3] = registers[3] & 0xFF;
    data->placa[4] = (registers[4] >> 8) & 0xFF;
    data->placa[5] = registers[4] & 0xFF;
    data->placa[6] = (registers[5] >> 8) & 0xFF;
    data->placa[7] = registers[5] & 0xFF;
    data->placa[8] = '\0'; // Null terminator
    
    // Confiança (offset 6)
    data->confianca = registers[6] & 0xFF;
    
    // Erro (offset 7)
    data->erro = registers[7] & 0xFF;
    
    return true;
}

/**
 * @brief Aguarda processamento da placa (polling no status)
 */
LPRStatus lpr_wait_processing(int fd, uint8_t camera_addr, int timeout_ms) {
    struct timeval start, now;
    gettimeofday(&start, NULL);
    
    while (1) {
        uint16_t status_reg;
        
        // Lê apenas o status (offset 0)
        if (modbus_read_holding_registers(fd, camera_addr, 0, 1, &status_reg)) {
            LPRStatus status = (LPRStatus)(status_reg & 0xFF);
            
            if (status == LPR_STATUS_OK || status == LPR_STATUS_ERRO) {
                return status;
            }
        }
        
        // Verifica timeout
        gettimeofday(&now, NULL);
        long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 + 
                         (now.tv_usec - start.tv_usec) / 1000;
        
        if (elapsed_ms >= timeout_ms) {
            fprintf(stderr, "[LPR 0x%02X] Timeout aguardando processamento\n", camera_addr);
            return LPR_STATUS_ERRO;
        }
        
        usleep(100000); // Aguarda 100ms entre tentativas (conforme especificação)
    }
}

/**
 * @brief Zera o trigger da câmera
 */
bool lpr_reset_trigger(int fd, uint8_t camera_addr) {
    uint16_t trigger_value = 0;
    return modbus_write_multiple_registers(fd, camera_addr, 1, 1, &trigger_value);
}

// ========== Funções específicas para Placar (0x20) ==========

/**
 * @brief Atualiza os dados no placar MODBUS
 */
bool placar_update(int fd, PlacarData *data) {
    uint16_t registers[13];
    
    registers[0] = data->vagas_livres_terreo_pne;
    registers[1] = data->vagas_livres_terreo_idoso;
    registers[2] = data->vagas_livres_terreo_comuns;
    registers[3] = data->vagas_livres_a1_pne;
    registers[4] = data->vagas_livres_a1_idoso;
    registers[5] = data->vagas_livres_a1_comuns;
    registers[6] = data->vagas_livres_a2_pne;
    registers[7] = data->vagas_livres_a2_idoso;
    registers[8] = data->vagas_livres_a2_comuns;
    registers[9] = data->num_carros_terreo;
    registers[10] = data->num_carros_a1;
    registers[11] = data->num_carros_a2;
    registers[12] = data->flags;
    
    bool success = modbus_write_multiple_registers(fd, MODBUS_ADDR_PLACAR, 0, 13, registers);
    
    if (success) {
        printf("[PLACAR] Dados atualizados (Flags: 0x%04X)\n", data->flags);
    } else {
        fprintf(stderr, "[PLACAR] Erro ao atualizar dados\n");
    }
    
    return success;
}

/**
 * @brief Lê os dados do placar MODBUS
 */
bool placar_read(int fd, PlacarData *data) {
    uint16_t registers[13];
    
    if (!modbus_read_holding_registers(fd, MODBUS_ADDR_PLACAR, 0, 13, registers)) {
        return false;
    }
    
    data->vagas_livres_terreo_pne = registers[0];
    data->vagas_livres_terreo_idoso = registers[1];
    data->vagas_livres_terreo_comuns = registers[2];
    data->vagas_livres_a1_pne = registers[3];
    data->vagas_livres_a1_idoso = registers[4];
    data->vagas_livres_a1_comuns = registers[5];
    data->vagas_livres_a2_pne = registers[6];
    data->vagas_livres_a2_idoso = registers[7];
    data->vagas_livres_a2_comuns = registers[8];
    data->num_carros_terreo = registers[9];
    data->num_carros_a1 = registers[10];
    data->num_carros_a2 = registers[11];
    data->flags = registers[12];
    
    return true;
}

