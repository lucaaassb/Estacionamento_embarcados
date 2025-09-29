#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Estruturas para mensagens JSON conforme especificação

// Evento de Entrada OK
typedef struct {
    char tipo[20];        // "entrada_ok"
    char placa[9];        // "ABC1D23"
    int conf;             // 86
    char ts[32];          // "2025-09-03T12:34:56Z"
    int andar;            // 1
} entrada_ok_t;

// Evento de Saída OK
typedef struct {
    char tipo[20];        // "saida_ok"
    char placa[9];        // "ABC1D23"
    int conf;             // 83
    char ts[32];          // "2025-09-03T15:10:00Z"
    int andar;            // 1
} saida_ok_t;

// Flags de status
typedef struct {
    int lotado;           // false/true
    int bloq2;            // false/true
} flags_t;

// Atualização de Vagas
typedef struct {
    char tipo[20];        // "vaga_status"
    int livres_a1;        // 5
    int livres_a2;        // 7
    int livres_total;     // 12
    flags_t flags;        // {"lotado": false, "bloq2": false}
} vaga_status_t;

// Evento de Passagem
typedef struct {
    char tipo[20];        // "passagem"
    int andar_origem;     // 1
    int andar_destino;    // 2
    char ts[32];          // "2025-09-03T12:34:56Z"
} passagem_t;

// Evento de Alerta
typedef struct {
    char tipo[20];        // "alerta"
    char mensagem[128];   // "Carro sem correspondência"
    char placa[9];        // "ABC1D23"
    char ts[32];          // "2025-09-03T12:34:56Z"
} alerta_t;

// Funções de serialização JSON
int serialize_entrada_ok(const entrada_ok_t* entrada, char* json_buffer, size_t buffer_size);
int serialize_saida_ok(const saida_ok_t* saida, char* json_buffer, size_t buffer_size);
int serialize_vaga_status(const vaga_status_t* status, char* json_buffer, size_t buffer_size);
int serialize_passagem(const passagem_t* passagem, char* json_buffer, size_t buffer_size);
int serialize_alerta(const alerta_t* alerta, char* json_buffer, size_t buffer_size);

// Funções de deserialização JSON
int deserialize_entrada_ok(const char* json_buffer, entrada_ok_t* entrada);
int deserialize_saida_ok(const char* json_buffer, saida_ok_t* saida);
int deserialize_vaga_status(const char* json_buffer, vaga_status_t* status);
int deserialize_passagem(const char* json_buffer, passagem_t* passagem);
int deserialize_alerta(const char* json_buffer, alerta_t* alerta);

// Funções auxiliares
void get_current_timestamp(char* timestamp_buffer, size_t buffer_size);
int send_json_message(int socket, const char* json_message);
int receive_json_message(int socket, char* json_buffer, size_t buffer_size);

#endif // JSON_UTILS_H
