#include "../inc/lpr_terreo.h"
#include "../inc/modbus.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

// File descriptors das câmeras LPR (compartilhados com o térreo)
int lpr_entrada_fd = -1;
int lpr_saida_fd = -1;

// Mutexes para acesso seguro às câmeras
pthread_mutex_t mutex_lpr_entrada = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_lpr_saida = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Inicializa câmeras LPR
 */
bool lpr_init(const char *porta_serial) {
    // Inicializa comunicação MODBUS
    lpr_entrada_fd = modbus_init(porta_serial);
    
    if(lpr_entrada_fd < 0) {
        fprintf(stderr, "[LPR] Erro ao inicializar porta serial %s\n", porta_serial);
        fprintf(stderr, "[LPR] Sistema funcionará SEM reconhecimento de placas\n");
        fprintf(stderr, "[LPR] Todos os carros receberão tickets temporários\n");
        return false;
    }
    
    // Usa o mesmo file descriptor para ambas as câmeras (mesmo barramento RS485)
    lpr_saida_fd = lpr_entrada_fd;
    
    printf("[LPR] Câmeras LPR inicializadas com sucesso\n");
    printf("[LPR] - Câmera Entrada: 0x%02X\n", MODBUS_ADDR_LPR_ENTRADA);
    printf("[LPR] - Câmera Saída: 0x%02X\n", MODBUS_ADDR_LPR_SAIDA);
    printf("[LPR] - Limiar de confiança: %d%%\n", LPR_CONFIANCA_LIMIAR);
    
    return true;
}

/**
 * @brief Finaliza câmeras LPR
 */
void lpr_cleanup() {
    if(lpr_entrada_fd >= 0) {
        modbus_close(lpr_entrada_fd);
        lpr_entrada_fd = -1;
        lpr_saida_fd = -1;
        printf("[LPR] Câmeras LPR finalizadas\n");
    }
}

/**
 * @brief Processa entrada de um carro com LPR
 */
bool lpr_processar_entrada(int numeroCarro, char *placa_out, int *confianca_out) {
    if(lpr_entrada_fd < 0) {
        // Modo degradado: sem LPR
        strcpy(placa_out, "");
        *confianca_out = 0;
        return false;
    }
    
    pthread_mutex_lock(&mutex_lpr_entrada);
    
    printf("[LPR-Entrada] Processando carro #%d...\n", numeroCarro);
    
    // Passo 1: Dispara trigger na câmera de entrada (0x11)
    if(!lpr_trigger_capture(lpr_entrada_fd, MODBUS_ADDR_LPR_ENTRADA)) {
        pthread_mutex_unlock(&mutex_lpr_entrada);
        fprintf(stderr, "[LPR-Entrada] Erro ao disparar trigger\n");
        strcpy(placa_out, "");
        *confianca_out = 0;
        return false;
    }
    
    // Passo 2: Aguarda processamento (polling no status) com timeout de 2s
    LPRStatus status = lpr_wait_processing(lpr_entrada_fd, MODBUS_ADDR_LPR_ENTRADA, 2000);
    
    if(status != LPR_STATUS_OK) {
        pthread_mutex_unlock(&mutex_lpr_entrada);
        fprintf(stderr, "[LPR-Entrada] Erro ou timeout no processamento (status=%d)\n", status);
        strcpy(placa_out, "");
        *confianca_out = 0;
        
        // Zera trigger mesmo em caso de erro
        lpr_reset_trigger(lpr_entrada_fd, MODBUS_ADDR_LPR_ENTRADA);
        return false;
    }
    
    // Passo 3: Lê dados da câmera (placa e confiança)
    LPRData data;
    if(!lpr_read_data(lpr_entrada_fd, MODBUS_ADDR_LPR_ENTRADA, &data)) {
        pthread_mutex_unlock(&mutex_lpr_entrada);
        fprintf(stderr, "[LPR-Entrada] Erro ao ler dados da câmera\n");
        strcpy(placa_out, "");
        *confianca_out = 0;
        lpr_reset_trigger(lpr_entrada_fd, MODBUS_ADDR_LPR_ENTRADA);
        return false;
    }
    
    // Passo 4: Zera trigger (conforme especificação)
    lpr_reset_trigger(lpr_entrada_fd, MODBUS_ADDR_LPR_ENTRADA);
    
    pthread_mutex_unlock(&mutex_lpr_entrada);
    
    // Copia resultados
    strncpy(placa_out, data.placa, 8);
    placa_out[8] = '\0';
    *confianca_out = data.confianca;
    
    // Log do resultado
    if(data.confianca >= LPR_CONFIANCA_LIMIAR) {
        printf("[LPR-Entrada] ✅ Placa lida: %s (confiança: %d%%)\n", data.placa, data.confianca);
    } else {
        printf("[LPR-Entrada] ⚠️  Baixa confiança: %s (%d%%) - será criado ticket temporário\n", 
               data.placa, data.confianca);
    }
    
    return true;
}

/**
 * @brief Processa saída de um carro com LPR
 */
bool lpr_processar_saida(char *placa_out, int *confianca_out) {
    if(lpr_saida_fd < 0) {
        // Modo degradado: sem LPR
        strcpy(placa_out, "");
        *confianca_out = 0;
        return false;
    }
    
    pthread_mutex_lock(&mutex_lpr_saida);
    
    printf("[LPR-Saída] Processando saída...\n");
    
    // Passo 1: Dispara trigger na câmera de saída (0x12)
    if(!lpr_trigger_capture(lpr_saida_fd, MODBUS_ADDR_LPR_SAIDA)) {
        pthread_mutex_unlock(&mutex_lpr_saida);
        fprintf(stderr, "[LPR-Saída] Erro ao disparar trigger\n");
        strcpy(placa_out, "");
        *confianca_out = 0;
        return false;
    }
    
    // Passo 2: Aguarda processamento (polling no status) com timeout de 2s
    LPRStatus status = lpr_wait_processing(lpr_saida_fd, MODBUS_ADDR_LPR_SAIDA, 2000);
    
    if(status != LPR_STATUS_OK) {
        pthread_mutex_unlock(&mutex_lpr_saida);
        fprintf(stderr, "[LPR-Saída] Erro ou timeout no processamento (status=%d)\n", status);
        strcpy(placa_out, "");
        *confianca_out = 0;
        lpr_reset_trigger(lpr_saida_fd, MODBUS_ADDR_LPR_SAIDA);
        return false;
    }
    
    // Passo 3: Lê dados da câmera (placa e confiança)
    LPRData data;
    if(!lpr_read_data(lpr_saida_fd, MODBUS_ADDR_LPR_SAIDA, &data)) {
        pthread_mutex_unlock(&mutex_lpr_saida);
        fprintf(stderr, "[LPR-Saída] Erro ao ler dados da câmera\n");
        strcpy(placa_out, "");
        *confianca_out = 0;
        lpr_reset_trigger(lpr_saida_fd, MODBUS_ADDR_LPR_SAIDA);
        return false;
    }
    
    // Passo 4: Zera trigger
    lpr_reset_trigger(lpr_saida_fd, MODBUS_ADDR_LPR_SAIDA);
    
    pthread_mutex_unlock(&mutex_lpr_saida);
    
    // Copia resultados
    strncpy(placa_out, data.placa, 8);
    placa_out[8] = '\0';
    *confianca_out = data.confianca;
    
    // Log do resultado
    if(data.confianca >= LPR_CONFIANCA_LIMIAR) {
        printf("[LPR-Saída] ✅ Placa lida: %s (confiança: %d%%)\n", data.placa, data.confianca);
    } else {
        printf("[LPR-Saída] ⚠️  Baixa confiança: %s (%d%%)\n", data.placa, data.confianca);
    }
    
    return true;
}

