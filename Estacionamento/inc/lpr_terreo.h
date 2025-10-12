#ifndef LPR_TERREO_H
#define LPR_TERREO_H

#include <stdbool.h>
#include "modbus.h"

// Limiar de confiança para aceitar placa (conforme especificação)
#define LPR_CONFIANCA_LIMIAR  70

// File descriptors globais para as câmeras LPR
extern int lpr_entrada_fd;
extern int lpr_saida_fd;

/**
 * @brief Inicializa câmeras LPR
 * @param porta_serial Porta serial MODBUS (ex: "/dev/ttyUSB0")
 * @return true se sucesso, false se erro
 */
bool lpr_init(const char *porta_serial);

/**
 * @brief Finaliza câmeras LPR
 */
void lpr_cleanup();

/**
 * @brief Processa entrada de um carro com LPR
 * @param numeroCarro ID do carro que está entrando
 * @param placa_out Buffer para armazenar a placa lida (saída)
 * @param confianca_out Ponteiro para armazenar a confiança (saída)
 * @return true se placa foi lida, false se não foi possível ler
 * 
 * Fluxo (conforme especificação):
 * 1. Sensor detecta carro → dispara Trigger na câmera 0x11
 * 2. Faz polling no Status até 2=OK ou 3=Erro (timeout 2s)
 * 3. Se OK, lê Placa e Confiança
 * 4. Zera Trigger
 * 5. Retorna placa e confiança
 */
bool lpr_processar_entrada(int numeroCarro, char *placa_out, int *confianca_out);

/**
 * @brief Processa saída de um carro com LPR
 * @param placa_out Buffer para armazenar a placa lida (saída)
 * @param confianca_out Ponteiro para armazenar a confiança (saída)
 * @return true se placa foi lida, false se não foi possível ler
 * 
 * Fluxo (conforme especificação):
 * 1. Sensor detecta carro → dispara Trigger na câmera 0x12
 * 2. Obtém Placa e confiança
 * 3. Zera Trigger
 * 4. Retorna placa e confiança
 */
bool lpr_processar_saida(char *placa_out, int *confianca_out);

#endif // LPR_TERREO_H

