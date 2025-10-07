#ifndef THINGSBOARD_H
#define THINGSBOARD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <time.h>
#include <pthread.h>

// Configurações do ThingsBoard
#define TB_SERVER_URL "https://tb.fse.lappis.rocks"
#define TB_ACCESS_TOKEN_TERREO "YOUR_TERREO_TOKEN"
#define TB_ACCESS_TOKEN_ANDAR1 "YOUR_ANDAR1_TOKEN"
#define TB_ACCESS_TOKEN_ANDAR2 "YOUR_ANDAR2_TOKEN"

// Estrutura para dados de telemetria
typedef struct {
    char device_token[64];
    char json_payload[1024];
} telemetry_data_t;

// Estrutura para resposta HTTP
typedef struct {
    char *memory;
    size_t size;
} http_response_t;

// Funções principais
int init_thingsboard_client();
void cleanup_thingsboard_client();

// Envio de telemetria
int send_telemetry_data(const char* device_token, const char* json_data);
int send_occupancy_data(const char* device_token, int andar, int* vagas_ocupadas, int num_vagas);
int send_vehicle_event(const char* device_token, const char* event_type, const char* placa, int vaga, float valor);
int send_sensor_data(const char* device_token, const char* sensor_name, int value);

// Envio de atributos
int send_device_attributes(const char* device_token, const char* json_attributes);

// Funções específicas para cada andar
int send_terreo_data(int* vagas_ocupadas, int carros_entrada, int carros_saida, float valor_arrecadado);
int send_andar1_data(int* vagas_ocupadas, int passagem_subindo, int passagem_descendo);
int send_andar2_data(int* vagas_ocupadas, int passagem_subindo, int passagem_descendo);

// Funcões utilitárias
size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, http_response_t *response);
char* create_telemetry_json(const char* key, const char* value);
char* create_multiple_telemetry_json(const char** keys, const char** values, int count);

// Thread para envio periódico
void* thingsboard_sender_thread(void* arg);

#endif // THINGSBOARD_H
