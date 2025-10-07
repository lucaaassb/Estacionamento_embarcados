#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

// Estrutura para dados do estacionamento
typedef struct {
    int dados_terreo[23];
    int dados_andar1[23];
    int dados_andar2[23];
    int comandos_enviar[5];
    pthread_mutex_t mutex;
} estacionamento_data_t;

// Funções do servidor HTTP
int init_http_server(int port);
void* http_server_thread(void* arg);
void send_http_response(int client_sock, int status_code, const char* content_type, const char* body);
void handle_status_request(int client_sock, estacionamento_data_t* data);
void handle_entrada_request(int client_sock, estacionamento_data_t* data);
void handle_saida_request(int client_sock, estacionamento_data_t* data);
void parse_http_request(const char* request, char* method, char* path);
void update_estacionamento_data(estacionamento_data_t* data, int* terreo, int* andar1, int* andar2, int* comandos);

#endif
