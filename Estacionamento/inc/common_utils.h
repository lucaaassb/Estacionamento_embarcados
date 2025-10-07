#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>

// Constantes do sistema
#define PRECO_POR_MINUTO 0.15f

// Estrutura para vaga de estacionamento
typedef struct {
    int numero_vaga;
    int numero_carro;
    bool ocupada;
    bool ocupado;  // Para compatibilidade com código existente
    struct timeval horario_entrada;
    struct timeval horario_saida;
    int tempo_permanencia_minutos;
    char placa_veiculo[9];
    int confianca_leitura;
} vaga_estacionamento_t;


// Estrutura para tickets temporários
typedef struct {
    int ticket_id;
    char placa_temporaria[9];
    int confianca;
    time_t timestamp;
    int vaga_associada;
    int andar;
    bool ativo;
} ticket_temporario_t;

// Estrutura para alertas de auditoria
typedef struct {
    time_t timestamp;
    char placa_veiculo[9];
    char motivo[128];
    int tipo_alerta; // 1=Sem correspondência entrada, 2=Placa inválida, 3=Erro sistema
    bool resolvido;
} alerta_auditoria_t;

// Estrutura de evento do sistema
typedef struct {
    time_t timestamp;
    int tipo_evento; // 1=entrada, 2=saida, 3=passagem, 4=alerta, 5=comando
    int andar_origem;
    int andar_destino;
    int numero_carro;
    int numero_vaga;
    char placa_veiculo[9];
    float valor_pago;
    int confianca_leitura;
    char mensagem[128]; // Para alertas e comandos
} evento_sistema_t;

// Níveis de log
#define LOG_ERRO 1
#define LOG_INFO 2
#define LOG_DEBUG 3

// Estrutura para sistema de logs
typedef struct {
    FILE* arquivo_log;
    pthread_mutex_t mutex_log;
    int nivel_log;
    char nome_arquivo[256];
} sistema_log_t;

// Funções de log
void init_log_system();
void log_evento(const char* mensagem, int nivel);
void log_erro(const char* mensagem);
void log_info(const char* mensagem);
void log_debug(const char* mensagem);
void close_log_system();
void rotacionar_log_se_necessario();

// Funções de tempo
int calcular_tempo_permanencia(struct timeval entrada, struct timeval saida);
float calcular_valor_pagamento(int tempo_minutos);
void obter_timestamp_atual(struct timeval* tv);

// Funções de validação
bool validar_placa(const char* placa);


// Funções de arquivo
int salvar_evento_arquivo(const evento_sistema_t* evento);

// Funções de tickets temporários
int gerar_ticket_temporario(const char* placa, int confianca, int vaga, int andar);
ticket_temporario_t* buscar_ticket_por_placa(const char* placa);
ticket_temporario_t* buscar_ticket_por_id(int ticket_id);
void desativar_ticket(int ticket_id);
int listar_tickets_ativos(ticket_temporario_t* tickets, int max_tickets);

// Funções de alertas de auditoria
void sinalizar_alerta_auditoria(const char* placa, const char* motivo, int tipo_alerta);
int buscar_correspondencia_entrada(const char* placa_saida);
void listar_alertas_pendentes(alerta_auditoria_t* alertas, int max_alertas);
void resolver_alerta_auditoria(int alerta_id);

// Funções de validação de correspondência
bool validar_correspondencia_entrada_saida(const char* placa_entrada, const char* placa_saida);
int buscar_entrada_por_placa(const char* placa);

// Funções de thread safety
pthread_mutex_t* get_log_mutex();
pthread_mutex_t* get_file_mutex();


#endif
