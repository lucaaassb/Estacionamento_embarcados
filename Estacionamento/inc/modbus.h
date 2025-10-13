#ifndef MODBUS_H
#define MODBUS_H

#include <stdint.h>
#include <stdbool.h>

// Endereços dos dispositivos MODBUS
#define MODBUS_ADDR_LPR_ENTRADA  0x11
#define MODBUS_ADDR_LPR_SAIDA    0x12
#define MODBUS_ADDR_PLACAR       0x20

// Funções MODBUS
#define MODBUS_FUNC_READ_HOLDING   0x03
#define MODBUS_FUNC_WRITE_MULTIPLE 0x10

// Configurações de comunicação serial
#define MODBUS_SERIAL_BAUD      115200
#define MODBUS_SERIAL_PORT      "/dev/serial0"  // Ajustar: ttyUSB0, ttyAMA0, serial0
#define MODBUS_TIMEOUT_MS       500
#define MODBUS_RETRIES          3
#define MODBUS_MIN_DELAY_MS     100
#define MODBUS_MEDIUM_DELAY_MS  250
#define MODBUS_MAX_DELAY_MS     500

// ⚠️ IMPORTANTE: Matrícula inserida em todas as mensagens MODBUS
// Conforme especificação: "É necessário enviar os 4 últimos dígitos da matrícula
// ao final de cada mensagem, sempre antes do CRC."
// Matrícula: 7700 (enviada como binário: 0, 7, 7, 0)
// Formato da mensagem: [Dados MODBUS] + [7700] + [CRC16]
#define MODBUS_MATRICULA_BYTE1 0x00  // 0
#define MODBUS_MATRICULA_BYTE2 0x07  // 7
#define MODBUS_MATRICULA_BYTE3 0x07  // 7
#define MODBUS_MATRICULA_BYTE4 0x00  // 0

// Array de matrícula (facilita o uso)
static const uint8_t MODBUS_MATRICULA[4] = {
    MODBUS_MATRICULA_BYTE1,
    MODBUS_MATRICULA_BYTE2,
    MODBUS_MATRICULA_BYTE3,
    MODBUS_MATRICULA_BYTE4
};

// Status da câmera LPR
typedef enum {
    LPR_STATUS_PRONTO       = 0,
    LPR_STATUS_PROCESSANDO  = 1,
    LPR_STATUS_OK           = 2,
    LPR_STATUS_ERRO         = 3
} LPRStatus;

// Estrutura para leitura de placa da câmera LPR
typedef struct {
    LPRStatus status;           // Status atual da câmera
    char placa[9];              // Placa lida (8 chars + \0)
    uint8_t confianca;          // Confiança (0-100%)
    uint8_t erro;               // Código de erro (0=none)
    bool trigger;               // Flag de trigger
} LPRData;

// Estrutura para dados do placar (0x20)
typedef struct {
    uint16_t vagas_livres_terreo_pne;      // Offset 0
    uint16_t vagas_livres_terreo_idoso;    // Offset 1
    uint16_t vagas_livres_terreo_comuns;   // Offset 2
    uint16_t vagas_livres_a1_pne;          // Offset 3
    uint16_t vagas_livres_a1_idoso;        // Offset 4
    uint16_t vagas_livres_a1_comuns;       // Offset 5
    uint16_t vagas_livres_a2_pne;          // Offset 6
    uint16_t vagas_livres_a2_idoso;        // Offset 7
    uint16_t vagas_livres_a2_comuns;       // Offset 8
    uint16_t num_carros_terreo;            // Offset 9
    uint16_t num_carros_a1;                // Offset 10
    uint16_t num_carros_a2;                // Offset 11
    uint16_t flags;                        // Offset 12: bit0=lotado geral, bit1=lotado 1º andar, bit2=lotado 2º andar
} PlacarData;

// Funções da biblioteca MODBUS

/**
 * @brief Inicializa a comunicação MODBUS serial
 * @param porta Caminho da porta serial (ex: "/dev/serial0")
 * @return file descriptor da porta serial, ou -1 em caso de erro
 */
int modbus_init(const char *porta);

/**
 * @brief Fecha a comunicação MODBUS
 * @param fd File descriptor da porta serial
 */
void modbus_close(int fd);

/**
 * @brief Calcula CRC16 MODBUS
 * @param buffer Buffer de dados
 * @param length Tamanho do buffer
 * @return CRC16 calculado
 */
uint16_t modbus_crc16(uint8_t *buffer, int length);

/**
 * @brief Lê holding registers (função 0x03)
 * @param fd File descriptor da porta serial
 * @param slave_addr Endereço do dispositivo escravo
 * @param start_addr Endereço inicial do registro
 * @param num_regs Número de registros a ler
 * @param output Buffer de saída para armazenar os dados lidos
 * @return true se sucesso, false se erro
 */
bool modbus_read_holding_registers(int fd, uint8_t slave_addr, uint16_t start_addr, 
                                   uint16_t num_regs, uint16_t *output);

/**
 * @brief Escreve múltiplos holding registers (função 0x10)
 * @param fd File descriptor da porta serial
 * @param slave_addr Endereço do dispositivo escravo
 * @param start_addr Endereço inicial do registro
 * @param num_regs Número de registros a escrever
 * @param data Buffer com os dados a escrever
 * @return true se sucesso, false se erro
 */
bool modbus_write_multiple_registers(int fd, uint8_t slave_addr, uint16_t start_addr,
                                     uint16_t num_regs, uint16_t *data);

// ========== Funções específicas para LPR ==========

/**
 * @brief Dispara captura de placa na câmera LPR
 * @param fd File descriptor da porta serial
 * @param camera_addr Endereço da câmera (0x11 entrada ou 0x12 saída)
 * @return true se sucesso, false se erro
 */
bool lpr_trigger_capture(int fd, uint8_t camera_addr);

/**
 * @brief Lê dados da câmera LPR (status, placa, confiança)
 * @param fd File descriptor da porta serial
 * @param camera_addr Endereço da câmera (0x11 entrada ou 0x12 saída)
 * @param data Estrutura para armazenar os dados lidos
 * @return true se sucesso, false se erro
 */
bool lpr_read_data(int fd, uint8_t camera_addr, LPRData *data);

/**
 * @brief Aguarda processamento da placa (polling no status)
 * @param fd File descriptor da porta serial
 * @param camera_addr Endereço da câmera
 * @param timeout_ms Timeout em milissegundos
 * @return Status final (OK, ERRO ou TIMEOUT)
 */
LPRStatus lpr_wait_processing(int fd, uint8_t camera_addr, int timeout_ms);

/**
 * @brief Zera o trigger da câmera (escreve 0 no offset 1)
 * @param fd File descriptor da porta serial
 * @param camera_addr Endereço da câmera
 * @return true se sucesso, false se erro
 */
bool lpr_reset_trigger(int fd, uint8_t camera_addr);

// ========== Funções específicas para Placar (0x20) ==========

/**
 * @brief Atualiza os dados no placar MODBUS
 * @param fd File descriptor da porta serial
 * @param data Estrutura com os dados a escrever
 * @return true se sucesso, false se erro
 */
bool placar_update(int fd, PlacarData *data);

/**
 * @brief Lê os dados do placar MODBUS
 * @param fd File descriptor da porta serial
 * @param data Estrutura para armazenar os dados lidos
 * @return true se sucesso, false se erro
 */
bool placar_read(int fd, PlacarData *data);

#endif // MODBUS_H

