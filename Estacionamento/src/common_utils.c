#include "common_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

// Variáveis globais para logs
static FILE* log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

// Arrays para tickets e alertas
static ticket_temporario_t tickets_temporarios[100];
static alerta_auditoria_t alertas_auditoria[100];
static int ticket_count = 0;
static int alerta_count = 0;

// Sistema de logs robusto
static sistema_log_t sistema_log = {0};

// Inicializar sistema de logs
void init_log_system() {
    pthread_mutex_init(&sistema_log.mutex_log, NULL);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    snprintf(sistema_log.nome_arquivo, sizeof(sistema_log.nome_arquivo),
             "logs/estacionamento_%04d%02d%02d.log", 
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);
    
    // Criar diretório de logs se não existir
    system("mkdir -p logs");
    
    sistema_log.arquivo_log = fopen(sistema_log.nome_arquivo, "a");
    if (!sistema_log.arquivo_log) {
        perror("Erro ao abrir arquivo de log");
    } else {
        sistema_log.nivel_log = LOG_DEBUG; // Log completo
        log_info("Sistema de logs inicializado");
    }
}

void rotacionar_log_se_necessario() {
    if (!sistema_log.arquivo_log) return;
    
    // Verificar tamanho do arquivo (rotacionar se > 10MB)
    fseek(sistema_log.arquivo_log, 0L, SEEK_END);
    long tamanho = ftell(sistema_log.arquivo_log);
    
    if (tamanho > 10 * 1024 * 1024) { // 10MB
        fclose(sistema_log.arquivo_log);
        
        char arquivo_backup[300];
        time_t now = time(NULL);
        snprintf(arquivo_backup, sizeof(arquivo_backup), "%s.%ld", 
                sistema_log.nome_arquivo, now);
        
        rename(sistema_log.nome_arquivo, arquivo_backup);
        
        sistema_log.arquivo_log = fopen(sistema_log.nome_arquivo, "a");
        if (sistema_log.arquivo_log) {
            log_info("Log rotacionado para arquivo de backup");
        }
    }
}

// Funções de log
void log_evento(const char* mensagem, int nivel) {
    if (nivel > sistema_log.nivel_log) return;
    
    pthread_mutex_lock(&sistema_log.mutex_log);
    
    time_t now = time(NULL);
    char timestamp[64];
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    const char* nivel_str[] = {"DEBUG", "INFO", "ERRO"};
    const char* nivel_atual = (nivel >= 1 && nivel <= 3) ? nivel_str[nivel-1] : "UNKNOWN";
    
    // Saída para console
    printf("[%s] %s: %s\n", timestamp, nivel_atual, mensagem);
    
    // Saída para arquivo
    if (sistema_log.arquivo_log) {
        fprintf(sistema_log.arquivo_log, "[%s] %s: %s\n", timestamp, nivel_atual, mensagem);
        fflush(sistema_log.arquivo_log);
        
        // Verificar rotação
        rotacionar_log_se_necessario();
    }
    
    pthread_mutex_unlock(&sistema_log.mutex_log);
}

void log_erro(const char* mensagem) {
    log_evento(mensagem, LOG_ERRO);
}

void log_info(const char* mensagem) {
    log_evento(mensagem, LOG_INFO);
}

void log_debug(const char* mensagem) {
    log_evento(mensagem, LOG_DEBUG);
}

void close_log_system() {
    pthread_mutex_lock(&sistema_log.mutex_log);
    
    if (sistema_log.arquivo_log) {
        log_info("Sistema de logs finalizando");
        fclose(sistema_log.arquivo_log);
        sistema_log.arquivo_log = NULL;
    }
    
    pthread_mutex_unlock(&sistema_log.mutex_log);
    pthread_mutex_destroy(&sistema_log.mutex_log);
}

// Funções de tempo
int calcular_tempo_permanencia(struct timeval entrada, struct timeval saida) {
    long diff_sec = saida.tv_sec - entrada.tv_sec;
    long diff_usec = saida.tv_usec - entrada.tv_usec;
    
    if (diff_usec < 0) {
        diff_sec--;
        diff_usec += 1000000;
    }
    
    int minutos = (int)(diff_sec / 60);
    if (diff_usec > 0) minutos++; // Arredondar para cima
    
    return minutos;
}

float calcular_valor_pagamento(int tempo_minutos) {
    return tempo_minutos * PRECO_POR_MINUTO;
}

void obter_timestamp_atual(struct timeval* tv) {
    gettimeofday(tv, NULL);
}

// Funções de validação
bool validar_placa(const char* placa) {
    if (!placa || strlen(placa) < 7 || strlen(placa) > 8) {
        return false;
    }
    
    // Validação básica: deve ter pelo menos 3 letras e 4 números
    int letras = 0, numeros = 0;
    for (int i = 0; placa[i]; i++) {
        if ((placa[i] >= 'A' && placa[i] <= 'Z') || 
            (placa[i] >= 'a' && placa[i] <= 'z')) {
            letras++;
        } else if (placa[i] >= '0' && placa[i] <= '9') {
            numeros++;
        } else {
            return false; // Caractere inválido
        }
    }
    
    return (letras >= 3 && numeros >= 4);
}

// Funções de arquivo
int salvar_evento_arquivo(const evento_sistema_t* evento) {
    pthread_mutex_lock(&file_mutex);
    
    FILE* file = fopen("eventos_sistema.log", "a");
    if (!file) {
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }
    
    const char* tipo_str[] = {"", "ENTRADA", "SAIDA", "PASSAGEM", "ALERTA", "COMANDO"};
    fprintf(file, "[%ld] %s: Andar %d->%d, Carro %d, Vaga %d, Placa %s, Valor %.2f\n",
            evento->timestamp,
            tipo_str[evento->tipo_evento],
            evento->andar_origem,
            evento->andar_destino,
            evento->numero_carro,
            evento->numero_vaga,
            evento->placa_veiculo,
            evento->valor_pago);
    
    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    return 0;
}

// Funções de tickets temporários
int gerar_ticket_temporario(const char* placa, int confianca, int vaga, int andar) {
    if (ticket_count >= 100) return -1;
    
    ticket_temporario_t* ticket = &tickets_temporarios[ticket_count];
    ticket->ticket_id = ticket_count + 1;
    strncpy(ticket->placa_temporaria, placa, 8);
    ticket->placa_temporaria[8] = '\0';
    ticket->confianca = confianca;
    ticket->timestamp = time(NULL);
    ticket->vaga_associada = vaga;
    ticket->andar = andar;
    ticket->ativo = true;
    
    ticket_count++;
    {
        char log_msg[96];
        snprintf(log_msg, sizeof(log_msg), "Ticket temporário gerado: ID %d, Placa %s", ticket->ticket_id, placa);
        log_info(log_msg);
    }
    return ticket->ticket_id;
}

ticket_temporario_t* buscar_ticket_por_placa(const char* placa) {
    for (int i = 0; i < ticket_count; i++) {
        if (tickets_temporarios[i].ativo && 
            strcmp(tickets_temporarios[i].placa_temporaria, placa) == 0) {
            return &tickets_temporarios[i];
        }
    }
    return NULL;
}

ticket_temporario_t* buscar_ticket_por_id(int ticket_id) {
    for (int i = 0; i < ticket_count; i++) {
        if (tickets_temporarios[i].ticket_id == ticket_id) {
            return &tickets_temporarios[i];
        }
    }
    return NULL;
}

void desativar_ticket(int ticket_id) {
    ticket_temporario_t* ticket = buscar_ticket_por_id(ticket_id);
    if (ticket) {
        ticket->ativo = false;
        {
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "Ticket %d desativado", ticket_id);
            log_info(log_msg);
        }
    }
}

int listar_tickets_ativos(ticket_temporario_t* tickets, int max_tickets) {
    int count = 0;
    for (int i = 0; i < ticket_count && count < max_tickets; i++) {
        if (tickets_temporarios[i].ativo) {
            tickets[count] = tickets_temporarios[i];
            count++;
        }
    }
    return count;
}

// Funções de alertas de auditoria
void sinalizar_alerta_auditoria(const char* placa, const char* motivo, int tipo_alerta) {
    if (alerta_count >= 100) return;
    
    alerta_auditoria_t* alerta = &alertas_auditoria[alerta_count];
    alerta->timestamp = time(NULL);
    strncpy(alerta->placa_veiculo, placa, 8);
    alerta->placa_veiculo[8] = '\0';
    strncpy(alerta->motivo, motivo, 127);
    alerta->motivo[127] = '\0';
    alerta->tipo_alerta = tipo_alerta;
    alerta->resolvido = false;
    
    alerta_count++;
    {
        char log_msg[160];
        snprintf(log_msg, sizeof(log_msg), "Alerta de auditoria: %s - %s", placa, motivo);
        log_erro(log_msg);
    }
}

int buscar_correspondencia_entrada(const char* placa_saida) {
    (void)placa_saida;
    // Implementação simplificada - em um sistema real, seria uma busca em banco de dados
    // Por enquanto, sempre retorna 0 (sem correspondência)
    return 0;
}

void listar_alertas_pendentes(alerta_auditoria_t* alertas, int max_alertas) {
    int count = 0;
    for (int i = 0; i < alerta_count && count < max_alertas; i++) {
        if (!alertas_auditoria[i].resolvido) {
            alertas[count] = alertas_auditoria[i];
            count++;
        }
    }
}

void resolver_alerta_auditoria(int alerta_id) {
    for (int i = 0; i < alerta_count; i++) {
        if (alertas_auditoria[i].timestamp == alerta_id) {
            alertas_auditoria[i].resolvido = true;
            {
                char log_msg[64];
                snprintf(log_msg, sizeof(log_msg), "Alerta %d resolvido", alerta_id);
                log_info(log_msg);
            }
            break;
        }
    }
}

// Funções de validação de correspondência
bool validar_correspondencia_entrada_saida(const char* placa_entrada, const char* placa_saida) {
    return (placa_entrada && placa_saida && strcmp(placa_entrada, placa_saida) == 0);
}

int buscar_entrada_por_placa(const char* placa) {
    (void)placa;
    // Implementação simplificada - em um sistema real, seria uma busca em banco de dados
    return 0;
}

// Funções de thread safety
pthread_mutex_t* get_log_mutex() {
    return &log_mutex;
}

pthread_mutex_t* get_file_mutex() {
    return &file_mutex;
}