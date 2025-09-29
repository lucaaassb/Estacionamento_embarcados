#include "modbus_utils.h"
#include <time.h>

// Inicializa conexão MODBUS
modbus_t* init_modbus_connection(const char* device, int baudrate) {
    modbus_t* ctx = modbus_new_rtu(device, baudrate, 'N', 8, 1);
    if (ctx == NULL) {
        fprintf(stderr, "Erro ao criar contexto MODBUS: %s\n", modbus_strerror(errno));
        return NULL;
    }
    
    // Configurar timeout
    modbus_set_response_timeout(ctx, 0, 500000); // 500ms
    modbus_set_byte_timeout(ctx, 0, 100000);    // 100ms
    
    // Conectar
    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Erro ao conectar MODBUS: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return NULL;
    }
    
    printf("Conexão MODBUS estabelecida em %s\n", device);
    return ctx;
}

// Fecha conexão MODBUS
void close_modbus_connection(modbus_t* ctx) {
    if (ctx) {
        modbus_close(ctx);
        modbus_free(ctx);
    }
}

// Converte registros para string da placa
void convert_placa_from_registers(uint16_t* registers, char* placa) {
    int i, j = 0;
    for (i = 0; i < PLACA_REG_COUNT; i++) {
        // Cada registrador contém 2 caracteres em big-endian
        placa[j++] = (char)((registers[i] >> 8) & 0xFF);
        placa[j++] = (char)(registers[i] & 0xFF);
    }
    placa[8] = '\0'; // Null terminator
}

// Converte string da placa para registros
void convert_placa_to_registers(const char* placa, uint16_t* registers) {
    int i, j = 0;
    for (i = 0; i < PLACA_REG_COUNT; i++) {
        char c1 = (j < 8) ? placa[j++] : 0;
        char c2 = (j < 8) ? placa[j++] : 0;
        registers[i] = (c1 << 8) | c2;
    }
}

// Dispara captura na câmera
int trigger_camera_capture(modbus_t* ctx, int camera_addr) {
    uint16_t trigger_value = 1;
    
    if (modbus_set_slave(ctx, camera_addr) == -1) {
        fprintf(stderr, "Erro ao definir endereço do escravo: %s\n", modbus_strerror(errno));
        return -1;
    }
    
    if (modbus_write_register(ctx, TRIGGER_REG, trigger_value) == -1) {
        fprintf(stderr, "Erro ao disparar captura: %s\n", modbus_strerror(errno));
        return -1;
    }
    
    printf("Trigger enviado para câmera 0x%02X\n", camera_addr);
    return 0;
}

// Lê status da câmera
int read_camera_status(modbus_t* ctx, int camera_addr) {
    uint16_t status;
    
    if (modbus_set_slave(ctx, camera_addr) == -1) {
        return -1;
    }
    
    if (modbus_read_registers(ctx, STATUS_REG, 1, &status) == -1) {
        return -1;
    }
    
    return (int)status;
}

// Lê dados da câmera
int read_camera_data(modbus_t* ctx, int camera_addr, lpr_data_t* data) {
    uint16_t registers[8];
    
    if (modbus_set_slave(ctx, camera_addr) == -1) {
        return -1;
    }
    
    if (modbus_read_registers(ctx, STATUS_REG, 8, registers) == -1) {
        return -1;
    }
    
    data->status = registers[STATUS_REG];
    data->confianca = registers[CONFIANCA_REG];
    data->erro = registers[ERRO_REG];
    
    // Converter placa
    convert_placa_from_registers(&registers[PLACA_REG_START], data->placa);
    
    return 0;
}

// Captura placa com polling
int capture_license_plate(modbus_t* ctx, int camera_addr, lpr_data_t* data, int timeout_ms) {
    int status;
    int attempts = 0;
    int max_attempts = timeout_ms / 100; // Polling a cada 100ms
    
    // Disparar captura
    if (trigger_camera_capture(ctx, camera_addr) == -1) {
        return -1;
    }
    
    // Polling do status
    while (attempts < max_attempts) {
        usleep(100000); // 100ms
        attempts++;
        
        status = read_camera_status(ctx, camera_addr);
        if (status == -1) {
            continue; // Erro de comunicação, tentar novamente
        }
        
        if (status == CAMERA_OK) {
            // Sucesso, ler dados
            if (read_camera_data(ctx, camera_addr, data) == 0) {
                printf("Placa capturada: %s (confiança: %d%%)\n", data->placa, data->confianca);
                return 0;
            }
        } else if (status == CAMERA_ERRO) {
            fprintf(stderr, "Erro na câmera 0x%02X\n", camera_addr);
            return -1;
        }
        // Se status == CAMERA_PROCESSANDO, continuar polling
    }
    
    fprintf(stderr, "Timeout na captura da câmera 0x%02X\n", camera_addr);
    return -1;
}

// Atualiza dados do placar
int update_placar_data(modbus_t* ctx, const placar_data_t* data) {
    uint16_t registers[13];
    
    if (modbus_set_slave(ctx, PLACAR_ADDR) == -1) {
        return -1;
    }
    
    // Preparar registros
    registers[VAGAS_TERREO_PCD] = data->vagas_terreo_pcd;
    registers[VAGAS_TERREO_IDOSO] = data->vagas_terreo_idoso;
    registers[VAGAS_TERREO_COMUM] = data->vagas_terreo_comum;
    registers[VAGAS_ANDAR1_PCD] = data->vagas_andar1_pcd;
    registers[VAGAS_ANDAR1_IDOSO] = data->vagas_andar1_idoso;
    registers[VAGAS_ANDAR1_COMUM] = data->vagas_andar1_comum;
    registers[VAGAS_ANDAR2_PCD] = data->vagas_andar2_pcd;
    registers[VAGAS_ANDAR2_IDOSO] = data->vagas_andar2_idoso;
    registers[VAGAS_ANDAR2_COMUM] = data->vagas_andar2_comum;
    registers[VAGAS_TOTAL_PCD] = data->vagas_total_pcd;
    registers[VAGAS_TOTAL_IDOSO] = data->vagas_total_idoso;
    registers[VAGAS_TOTAL_COMUM] = data->vagas_total_comum;
    registers[FLAGS_REG] = data->flags;
    
    // Escrever múltiplos registros
    if (modbus_write_registers(ctx, 0, 13, registers) == -1) {
        fprintf(stderr, "Erro ao atualizar placar: %s\n", modbus_strerror(errno));
        return -1;
    }
    
    printf("Placar atualizado com sucesso\n");
    return 0;
}

// Lê dados do placar
int read_placar_data(modbus_t* ctx, placar_data_t* data) {
    uint16_t registers[13];
    
    if (modbus_set_slave(ctx, PLACAR_ADDR) == -1) {
        return -1;
    }
    
    if (modbus_read_registers(ctx, 0, 13, registers) == -1) {
        return -1;
    }
    
    data->vagas_terreo_pcd = registers[VAGAS_TERREO_PCD];
    data->vagas_terreo_idoso = registers[VAGAS_TERREO_IDOSO];
    data->vagas_terreo_comum = registers[VAGAS_TERREO_COMUM];
    data->vagas_andar1_pcd = registers[VAGAS_ANDAR1_PCD];
    data->vagas_andar1_idoso = registers[VAGAS_ANDAR1_IDOSO];
    data->vagas_andar1_comum = registers[VAGAS_ANDAR1_COMUM];
    data->vagas_andar2_pcd = registers[VAGAS_ANDAR2_PCD];
    data->vagas_andar2_idoso = registers[VAGAS_ANDAR2_IDOSO];
    data->vagas_andar2_comum = registers[VAGAS_ANDAR2_COMUM];
    data->vagas_total_pcd = registers[VAGAS_TOTAL_PCD];
    data->vagas_total_idoso = registers[VAGAS_TOTAL_IDOSO];
    data->vagas_total_comum = registers[VAGAS_TOTAL_COMUM];
    data->flags = registers[FLAGS_REG];
    
    return 0;
}

// Função genérica para retry de operações MODBUS
int modbus_retry_operation(modbus_t* ctx, int (*operation)(modbus_t*, void*), void* data, int max_retries) {
    int i;
    for (i = 0; i < max_retries; i++) {
        if (operation(ctx, data) == 0) {
            return 0; // Sucesso
        }
        
        if (i < max_retries - 1) {
            usleep(100000 * (i + 1)); // Backoff exponencial
        }
    }
    return -1; // Falha após todas as tentativas
}
