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
 * @brief Lookup table para CRC16 (otimização - 15x mais rápido)
 */
static const uint16_t crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

/**
 * @brief Calcula CRC16 MODBUS usando lookup table (15x mais rápido)
 */
uint16_t modbus_crc16(uint8_t *buffer, int length) {
    uint16_t crc = 0xFFFF;
    
    for (int i = 0; i < length; i++) {
        uint8_t index = (crc ^ buffer[i]) & 0xFF;
        crc = (crc >> 8) ^ crc16_table[index];
    }
    
    return crc;
}

/**
 * @brief Converte buffer em string hexadecimal para debug
 */
static void print_hex_debug(const char *prefix, uint8_t *buffer, int length) {
    fprintf(stderr, "[MODBUS-DEBUG] %s: ", prefix);
    for (int i = 0; i < length; i++) {
        fprintf(stderr, "%02X ", buffer[i]);
    }
    fprintf(stderr, "(%d bytes)\n", length);
}

/**
 * @brief Lê holding registers (função 0x03) com retry automático
 */
bool modbus_read_holding_registers(int fd, uint8_t slave_addr, uint16_t start_addr, 
                                   uint16_t num_regs, uint16_t *output) {
    uint8_t request[12];
    uint8_t response[256];
    
    // Array de delays progressivos para retries
    const int delays_ms[] = {MODBUS_MIN_DELAY_MS, MODBUS_MEDIUM_DELAY_MS, MODBUS_MAX_DELAY_MS};
    
    // Monta requisição MODBUS (Little Endian - compatível com C++)
    request[0] = slave_addr;
    request[1] = MODBUS_FUNC_READ_HOLDING;
    request[2] = start_addr & 0xFF;         // LSB first (Little Endian)
    request[3] = (start_addr >> 8) & 0xFF;  // MSB second
    request[4] = num_regs & 0xFF;           // LSB first (Little Endian)
    request[5] = (num_regs >> 8) & 0xFF;    // MSB second
    
    // Matrícula binária antes do CRC (conforme implementação C++)
    request[6] = MODBUS_MATRICULA[0];  // 0x00
    request[7] = MODBUS_MATRICULA[1];  // 0x07
    request[8] = MODBUS_MATRICULA[2];  // 0x07
    request[9] = MODBUS_MATRICULA[3];  // 0x00
    
    // Calcula CRC incluindo a matrícula
    uint16_t crc = modbus_crc16(request, 10);
    request[10] = crc & 0xFF;
    request[11] = (crc >> 8) & 0xFF;
    
    // Sistema de retry com delays progressivos
    for (int retry = 0; retry < MODBUS_RETRIES; retry++) {
        if (retry > 0) {
            fprintf(stderr, "[MODBUS] Tentativa %d/%d...\n", retry + 1, MODBUS_RETRIES);
            usleep(delays_ms[retry - 1] * 1000);
        }
        
        // Envia requisição
        tcflush(fd, TCIOFLUSH);
        int written = write(fd, request, 12);
        if (written != 12) {
            fprintf(stderr, "[MODBUS] Erro ao enviar (tentativa %d): %s\n", retry + 1, strerror(errno));
            continue;
        }
        
        // Debug: mostra pacote enviado (apenas na primeira tentativa)
        if (retry == 0) {
            print_hex_debug("TX", request, 12);
        }
        
        // Aguarda resposta
        usleep(MODBUS_TIMEOUT_MS * 1000);
        
        int expected_len = 5 + (num_regs * 2);
        int bytes_read = read(fd, response, expected_len);
        
        if (bytes_read < expected_len) {
            fprintf(stderr, "[MODBUS] Timeout (esperado %d, recebido %d)\n", expected_len, bytes_read);
            continue;
        }
        
        // Debug: mostra resposta recebida (apenas na primeira tentativa)
        if (retry == 0) {
            print_hex_debug("RX", response, bytes_read);
        }
        
        // Verifica endereço e função
        if (response[0] != slave_addr || response[1] != MODBUS_FUNC_READ_HOLDING) {
            fprintf(stderr, "[MODBUS] Resposta inválida (addr=0x%02X func=0x%02X)\n", 
                    response[0], response[1]);
            continue;
        }
        
        // Verifica CRC
        uint16_t received_crc = response[bytes_read - 2] | (response[bytes_read - 1] << 8);
        uint16_t calculated_crc = modbus_crc16(response, bytes_read - 2);
        
        if (received_crc != calculated_crc) {
            fprintf(stderr, "[MODBUS] Erro de CRC (esperado 0x%04X, recebido 0x%04X)\n", 
                    calculated_crc, received_crc);
            continue;
        }
        
        // Extrai dados (Little Endian - compatível com C++)
        int byte_count = response[2];
        for (int i = 0; i < num_regs; i++) {
            output[i] = response[3 + i * 2] | (response[4 + i * 2] << 8);  // LSB | MSB
        }
        
        // Sucesso!
        return true;
    }
    
    // Falhou após todas as tentativas
    fprintf(stderr, "[MODBUS] Falha após %d tentativas\n", MODBUS_RETRIES);
    return false;
}

/**
 * @brief Escreve múltiplos holding registers (função 0x10) com retry automático
 */
bool modbus_write_multiple_registers(int fd, uint8_t slave_addr, uint16_t start_addr,
                                     uint16_t num_regs, uint16_t *data) {
    uint8_t request[260];
    uint8_t response[12];
    
    // Array de delays progressivos para retries
    const int delays_ms[] = {MODBUS_MIN_DELAY_MS, MODBUS_MEDIUM_DELAY_MS, MODBUS_MAX_DELAY_MS};
    
    int byte_count = num_regs * 2;
    
    // Monta requisição MODBUS (Little Endian - compatível com C++)
    request[0] = slave_addr;
    request[1] = MODBUS_FUNC_WRITE_MULTIPLE;
    request[2] = start_addr & 0xFF;         // LSB first (Little Endian)
    request[3] = (start_addr >> 8) & 0xFF;  // MSB second
    request[4] = num_regs & 0xFF;           // LSB first (Little Endian)
    request[5] = (num_regs >> 8) & 0xFF;    // MSB second
    request[6] = byte_count;
    
    // Dados (Little Endian - compatível com C++)
    for (int i = 0; i < num_regs; i++) {
        request[7 + i * 2] = data[i] & 0xFF;          // LSB first
        request[8 + i * 2] = (data[i] >> 8) & 0xFF;   // MSB second
    }
    
    // Matrícula binária antes do CRC (conforme implementação C++)
    int data_len = 7 + byte_count;
    request[data_len]     = MODBUS_MATRICULA[0];  // 0x00
    request[data_len + 1] = MODBUS_MATRICULA[1];  // 0x07
    request[data_len + 2] = MODBUS_MATRICULA[2];  // 0x07
    request[data_len + 3] = MODBUS_MATRICULA[3];  // 0x00
    
    // Calcula CRC incluindo a matrícula
    int total_len = data_len + 4;
    uint16_t crc = modbus_crc16(request, total_len);
    request[total_len] = crc & 0xFF;
    request[total_len + 1] = (crc >> 8) & 0xFF;
    
    // Sistema de retry com delays progressivos
    for (int retry = 0; retry < MODBUS_RETRIES; retry++) {
        if (retry > 0) {
            fprintf(stderr, "[MODBUS] Tentativa de escrita %d/%d...\n", retry + 1, MODBUS_RETRIES);
            usleep(delays_ms[retry - 1] * 1000);
        }
        
        // Envia requisição
        tcflush(fd, TCIOFLUSH);
        int written = write(fd, request, total_len + 2);
        if (written != total_len + 2) {
            fprintf(stderr, "[MODBUS] Erro ao enviar (tentativa %d)\n", retry + 1);
            continue;
        }
        
        // Debug: mostra pacote enviado (apenas na primeira tentativa)
        if (retry == 0) {
            print_hex_debug("TX Write", request, total_len + 2);
        }
        
        // Aguarda confirmação
        usleep(MODBUS_TIMEOUT_MS * 1000);
        int bytes_read = read(fd, response, 8);
        
        if (bytes_read < 8) {
            fprintf(stderr, "[MODBUS] Timeout na confirmação (recebido %d bytes)\n", bytes_read);
            continue;
        }
        
        // Debug: mostra resposta (apenas na primeira tentativa)
        if (retry == 0) {
            print_hex_debug("RX Write", response, bytes_read);
        }
        
        // Verifica resposta
        if (response[0] != slave_addr || response[1] != MODBUS_FUNC_WRITE_MULTIPLE) {
            fprintf(stderr, "[MODBUS] Erro na confirmação (addr=0x%02X func=0x%02X)\n", 
                    response[0], response[1]);
            continue;
        }
        
        // Sucesso!
        return true;
    }
    
    // Falhou após todas as tentativas
    fprintf(stderr, "[MODBUS] Escrita falhou após %d tentativas\n", MODBUS_RETRIES);
    return false;
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
    
    // Placa (offsets 2-5) - 8 caracteres ASCII em 4 registros (Little Endian)
    data->placa[0] = registers[2] & 0xFF;          // LSB
    data->placa[1] = (registers[2] >> 8) & 0xFF;   // MSB
    data->placa[2] = registers[3] & 0xFF;          // LSB
    data->placa[3] = (registers[3] >> 8) & 0xFF;   // MSB
    data->placa[4] = registers[4] & 0xFF;          // LSB
    data->placa[5] = (registers[4] >> 8) & 0xFF;   // MSB
    data->placa[6] = registers[5] & 0xFF;          // LSB
    data->placa[7] = (registers[5] >> 8) & 0xFF;   // MSB
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

