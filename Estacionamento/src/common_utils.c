#include "common_utils.h"
#include <errno.h>
#include <sys/stat.h>

// Variáveis globais para thread safety
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE* log_file = NULL;

// Inicializa sistema de log
void init_log_system() {
    pthread_mutex_lock(&log_mutex);
    
    if (log_file == NULL) {
        log_file = fopen("estacionamento.log", "a");
        if (log_file == NULL) {
            fprintf(stderr, "Erro ao abrir arquivo de log: %s\n", strerror(errno));
        } else {
            // Configurar buffer de linha para logs em tempo real
            setlinebuf(log_file);
        }
    }
    
    pthread_mutex_unlock(&log_mutex);
}

// Log genérico
void log_evento(const char* mensagem, int nivel) {
    pthread_mutex_lock(&log_mutex);
    
    if (log_file) {
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        
        const char* nivel_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};
        fprintf(log_file, "[%s] [%s] %s\n", timestamp, nivel_str[nivel], mensagem);
        fflush(log_file);
    }
    
    // Também imprimir no console para debug
    printf("[LOG] %s\n", mensagem);
    
    pthread_mutex_unlock(&log_mutex);
}

// Log de erro
void log_erro(const char* mensagem) {
    log_evento(mensagem, 3);
}

// Log de informação
void log_info(const char* mensagem) {
    log_evento(mensagem, 1);
}

// Log de debug
void log_debug(const char* mensagem) {
    log_evento(mensagem, 0);
}

// Fecha sistema de log
void close_log_system() {
    pthread_mutex_lock(&log_mutex);
    
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    
    pthread_mutex_unlock(&log_mutex);
}

// Calcula tempo de permanência em minutos
int calcular_tempo_permanencia(struct timeval entrada, struct timeval saida) {
    long segundos = saida.tv_sec - entrada.tv_sec;
    long microsegundos = saida.tv_usec - entrada.tv_usec;
    
    if (microsegundos < 0) {
        segundos--;
        microsegundos += 1000000;
    }
    
    // Converter para minutos (arredondando para cima)
    int minutos = (int)(segundos / 60);
    if (segundos % 60 > 0 || microsegundos > 0) {
        minutos++;
    }
    
    return minutos;
}

// Calcula valor a ser pago
float calcular_valor_pagamento(int tempo_minutos) {
    return tempo_minutos * PRECO_POR_MINUTO;
}

// Obtém timestamp atual
void obter_timestamp_atual(struct timeval* tv) {
    gettimeofday(tv, NULL);
}

// Valida formato da placa (formato brasileiro: ABC1234)
bool validar_placa(const char* placa) {
    if (!placa || strlen(placa) != 7) {
        return false;
    }
    
    // Verificar se os primeiros 3 caracteres são letras
    for (int i = 0; i < 3; i++) {
        if (placa[i] < 'A' || placa[i] > 'Z') {
            return false;
        }
    }
    
    // Verificar se os últimos 4 caracteres são dígitos
    for (int i = 3; i < 7; i++) {
        if (placa[i] < '0' || placa[i] > '9') {
            return false;
        }
    }
    
    return true;
}



// Salva evento em arquivo
int salvar_evento_arquivo(const evento_sistema_t* evento) {
    pthread_mutex_lock(&file_mutex);
    
    FILE* file = fopen("eventos.log", "a");
    if (!file) {
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }
    
    fprintf(file, "%ld,%d,%d,%d,%d,%d,%s,%.2f,%d\n",
            evento->timestamp,
            evento->tipo_evento,
            evento->andar_origem,
            evento->andar_destino,
            evento->numero_carro,
            evento->numero_vaga,
            evento->placa_veiculo,
            evento->valor_pago,
            evento->confianca_leitura);
    
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    
    return 0;
}


// Getters para mutexes
pthread_mutex_t* get_log_mutex() {
    return &log_mutex;
}

pthread_mutex_t* get_file_mutex() {
    return &file_mutex;
}

// ===== SISTEMA DE TICKETS TEMPORÁRIOS =====

// Lista global de tickets temporários
static ticket_temporario_t tickets_temporarios[100];
static int proximo_ticket_id = 1;
static pthread_mutex_t tickets_mutex = PTHREAD_MUTEX_INITIALIZER;

// Gera um ticket temporário para placa não lida/baixa confiança
int gerar_ticket_temporario(const char* placa, int confianca, int vaga, int andar) {
    pthread_mutex_lock(&tickets_mutex);
    
    // Encontrar slot livre
    for (int i = 0; i < 100; i++) {
        if (!tickets_temporarios[i].ativo) {
            tickets_temporarios[i].ticket_id = proximo_ticket_id++;
            strncpy(tickets_temporarios[i].placa_temporaria, placa, 8);
            tickets_temporarios[i].placa_temporaria[8] = '\0';
            tickets_temporarios[i].confianca = confianca;
            tickets_temporarios[i].timestamp = time(NULL);
            tickets_temporarios[i].vaga_associada = vaga;
            tickets_temporarios[i].andar = andar;
            tickets_temporarios[i].ativo = true;
            
            log_info("Ticket temporário gerado: ID=%d, Placa=%s, Vaga=%d, Andar=%d", 
                    tickets_temporarios[i].ticket_id, placa, vaga, andar);
            
            pthread_mutex_unlock(&tickets_mutex);
            return tickets_temporarios[i].ticket_id;
        }
    }
    
    pthread_mutex_unlock(&tickets_mutex);
    log_erro("Não foi possível gerar ticket temporário - lista cheia");
    return -1;
}

// Busca ticket por placa
ticket_temporario_t* buscar_ticket_por_placa(const char* placa) {
    pthread_mutex_lock(&tickets_mutex);
    
    for (int i = 0; i < 100; i++) {
        if (tickets_temporarios[i].ativo && 
            strcmp(tickets_temporarios[i].placa_temporaria, placa) == 0) {
            pthread_mutex_unlock(&tickets_mutex);
            return &tickets_temporarios[i];
        }
    }
    
    pthread_mutex_unlock(&tickets_mutex);
    return NULL;
}

// Busca ticket por ID
ticket_temporario_t* buscar_ticket_por_id(int ticket_id) {
    pthread_mutex_lock(&tickets_mutex);
    
    for (int i = 0; i < 100; i++) {
        if (tickets_temporarios[i].ativo && 
            tickets_temporarios[i].ticket_id == ticket_id) {
            pthread_mutex_unlock(&tickets_mutex);
            return &tickets_temporarios[i];
        }
    }
    
    pthread_mutex_unlock(&tickets_mutex);
    return NULL;
}

// Desativa um ticket
void desativar_ticket(int ticket_id) {
    pthread_mutex_lock(&tickets_mutex);
    
    for (int i = 0; i < 100; i++) {
        if (tickets_temporarios[i].ativo && 
            tickets_temporarios[i].ticket_id == ticket_id) {
            tickets_temporarios[i].ativo = false;
            log_info("Ticket %d desativado", ticket_id);
            break;
        }
    }
    
    pthread_mutex_unlock(&tickets_mutex);
}

// Lista tickets ativos
int listar_tickets_ativos(ticket_temporario_t* tickets, int max_tickets) {
    pthread_mutex_lock(&tickets_mutex);
    
    int count = 0;
    for (int i = 0; i < 100 && count < max_tickets; i++) {
        if (tickets_temporarios[i].ativo) {
            tickets[count] = tickets_temporarios[i];
            count++;
        }
    }
    
    pthread_mutex_unlock(&tickets_mutex);
    return count;
}

// ===== SISTEMA DE ALERTAS DE AUDITORIA =====

// Lista global de alertas
static alerta_auditoria_t alertas_auditoria[50];
static int proximo_alerta_id = 1;
static pthread_mutex_t alertas_mutex = PTHREAD_MUTEX_INITIALIZER;

// Sinaliza alerta de auditoria
void sinalizar_alerta_auditoria(const char* placa, const char* motivo, int tipo_alerta) {
    pthread_mutex_lock(&alertas_mutex);
    
    // Encontrar slot livre
    for (int i = 0; i < 50; i++) {
        if (!alertas_auditoria[i].resolvido) {
            alertas_auditoria[i].timestamp = time(NULL);
            strncpy(alertas_auditoria[i].placa_veiculo, placa, 8);
            alertas_auditoria[i].placa_veiculo[8] = '\0';
            strncpy(alertas_auditoria[i].motivo, motivo, 127);
            alertas_auditoria[i].motivo[127] = '\0';
            alertas_auditoria[i].tipo_alerta = tipo_alerta;
            alertas_auditoria[i].resolvido = false;
            
            log_erro("ALERTA AUDITORIA: Placa=%s, Motivo=%s, Tipo=%d", 
                    placa, motivo, tipo_alerta);
            break;
        }
    }
    
    pthread_mutex_unlock(&alertas_mutex);
}

// Busca correspondência de entrada para uma placa de saída
int buscar_correspondencia_entrada(const char* placa_saida) {
    // Implementação simplificada - busca nos logs de eventos
    FILE* file = fopen("eventos.log", "r");
    if (!file) {
        return 0; // Não encontrou
    }
    
    char line[512];
    int encontrou = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "Tipo: 1") && strstr(line, placa_saida)) {
            encontrou = 1;
            break;
        }
    }
    
    fclose(file);
    return encontrou;
}

// Lista alertas pendentes
void listar_alertas_pendentes(alerta_auditoria_t* alertas, int max_alertas) {
    pthread_mutex_lock(&alertas_mutex);
    
    int count = 0;
    for (int i = 0; i < 50 && count < max_alertas; i++) {
        if (!alertas_auditoria[i].resolvido) {
            alertas[count] = alertas_auditoria[i];
            count++;
        }
    }
    
    pthread_mutex_unlock(&alertas_mutex);
}

// Resolve alerta de auditoria
void resolver_alerta_auditoria(int alerta_id) {
    pthread_mutex_lock(&alertas_mutex);
    
    for (int i = 0; i < 50; i++) {
        if (!alertas_auditoria[i].resolvido && 
            alertas_auditoria[i].timestamp == alerta_id) {
            alertas_auditoria[i].resolvido = true;
            log_info("Alerta de auditoria %d resolvido", alerta_id);
            break;
        }
    }
    
    pthread_mutex_unlock(&alertas_mutex);
}

// Valida correspondência entre entrada e saída
bool validar_correspondencia_entrada_saida(const char* placa_entrada, const char* placa_saida) {
    return (strcmp(placa_entrada, placa_saida) == 0);
}

// Busca entrada por placa
int buscar_entrada_por_placa(const char* placa) {
    return buscar_correspondencia_entrada(placa);
}
