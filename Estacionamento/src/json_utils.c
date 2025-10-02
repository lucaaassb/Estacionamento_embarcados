#include "json_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>

// Função auxiliar para obter timestamp atual
void get_current_timestamp(char* timestamp_buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    
    strftime(timestamp_buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}

// Função para enviar mensagem JSON via socket
int send_json_message(int socket, const char* json_message) {
    if (!json_message) return -1;
    
    size_t len = strlen(json_message);
    
    // Enviar tamanho da mensagem primeiro
    if (send(socket, &len, sizeof(size_t), 0) == -1) {
        return -1;
    }
    
    // Enviar mensagem JSON
    if (send(socket, json_message, len, 0) == -1) {
        return -1;
    }
    
    return 0;
}

// Função para receber mensagem JSON via socket
int receive_json_message(int socket, char* json_buffer, size_t buffer_size) {
    size_t len;
    
    // Receber tamanho da mensagem
    if (recv(socket, &len, sizeof(size_t), 0) == -1) {
        return -1;
    }
    
    if (len >= buffer_size) {
        return -1; // Buffer muito pequeno
    }
    
    // Receber mensagem JSON
    if (recv(socket, json_buffer, len, 0) == -1) {
        return -1;
    }
    
    json_buffer[len] = '\0';
    return 0;
}

// Serialização de entrada OK
int serialize_entrada_ok(const entrada_ok_t* entrada, char* json_buffer, size_t buffer_size) {
    if (!entrada || !json_buffer) return -1;
    
    int result = snprintf(json_buffer, buffer_size,
        "{\n"
        "  \"tipo\": \"%s\",\n"
        "  \"placa\": \"%s\",\n"
        "  \"conf\": %d,\n"
        "  \"ts\": \"%s\",\n"
        "  \"andar\": %d\n"
        "}",
        entrada->tipo,
        entrada->placa,
        entrada->conf,
        entrada->ts,
        entrada->andar);
    
    return (result < buffer_size) ? 0 : -1;
}

// Serialização de saída OK
int serialize_saida_ok(const saida_ok_t* saida, char* json_buffer, size_t buffer_size) {
    if (!saida || !json_buffer) return -1;
    
    int result = snprintf(json_buffer, buffer_size,
        "{\n"
        "  \"tipo\": \"%s\",\n"
        "  \"placa\": \"%s\",\n"
        "  \"conf\": %d,\n"
        "  \"ts\": \"%s\",\n"
        "  \"andar\": %d\n"
        "}",
        saida->tipo,
        saida->placa,
        saida->conf,
        saida->ts,
        saida->andar);
    
    return (result < buffer_size) ? 0 : -1;
}

// Serialização de status de vagas
int serialize_vaga_status(const vaga_status_t* status, char* json_buffer, size_t buffer_size) {
    if (!status || !json_buffer) return -1;
    
    int result = snprintf(json_buffer, buffer_size,
        "{\n"
        "  \"tipo\": \"%s\",\n"
        "  \"livres_a1\": %d,\n"
        "  \"livres_a2\": %d,\n"
        "  \"livres_total\": %d,\n"
        "  \"flags\": {\n"
        "    \"lotado\": %s,\n"
        "    \"bloq2\": %s\n"
        "  }\n"
        "}",
        status->tipo,
        status->livres_a1,
        status->livres_a2,
        status->livres_total,
        status->flags.lotado ? "true" : "false",
        status->flags.bloq2 ? "true" : "false");
    
    return (result < buffer_size) ? 0 : -1;
}

// Serialização de passagem
int serialize_passagem(const passagem_t* passagem, char* json_buffer, size_t buffer_size) {
    if (!passagem || !json_buffer) return -1;
    
    int result = snprintf(json_buffer, buffer_size,
        "{\n"
        "  \"tipo\": \"%s\",\n"
        "  \"andar_origem\": %d,\n"
        "  \"andar_destino\": %d,\n"
        "  \"ts\": \"%s\"\n"
        "}",
        passagem->tipo,
        passagem->andar_origem,
        passagem->andar_destino,
        passagem->ts);
    
    return (result < buffer_size) ? 0 : -1;
}

// Serialização de alerta
int serialize_alerta(const alerta_t* alerta, char* json_buffer, size_t buffer_size) {
    if (!alerta || !json_buffer) return -1;
    
    int result = snprintf(json_buffer, buffer_size,
        "{\n"
        "  \"tipo\": \"%s\",\n"
        "  \"mensagem\": \"%s\",\n"
        "  \"placa\": \"%s\",\n"
        "  \"ts\": \"%s\"\n"
        "}",
        alerta->tipo,
        alerta->mensagem,
        alerta->placa,
        alerta->ts);
    
    return (result < buffer_size) ? 0 : -1;
}

// Deserialização de entrada OK
int deserialize_entrada_ok(const char* json_buffer, entrada_ok_t* entrada) {
    if (!json_buffer || !entrada) return -1;
    
    // Implementação simplificada - em um sistema real, usar uma biblioteca JSON
    // Por enquanto, apenas inicializar com valores padrão
    strcpy(entrada->tipo, "entrada_ok");
    strcpy(entrada->placa, "UNKNOWN");
    entrada->conf = 0;
    get_current_timestamp(entrada->ts, sizeof(entrada->ts));
    entrada->andar = 0;
    
    return 0;
}

// Deserialização de saída OK
int deserialize_saida_ok(const char* json_buffer, saida_ok_t* saida) {
    if (!json_buffer || !saida) return -1;
    
    // Implementação simplificada
    strcpy(saida->tipo, "saida_ok");
    strcpy(saida->placa, "UNKNOWN");
    saida->conf = 0;
    get_current_timestamp(saida->ts, sizeof(saida->ts));
    saida->andar = 0;
    
    return 0;
}

// Deserialização de status de vagas
int deserialize_vaga_status(const char* json_buffer, vaga_status_t* status) {
    if (!json_buffer || !status) return -1;
    
    // Implementação simplificada
    strcpy(status->tipo, "vaga_status");
    status->livres_a1 = 0;
    status->livres_a2 = 0;
    status->livres_total = 0;
    status->flags.lotado = 0;
    status->flags.bloq2 = 0;
    
    return 0;
}

// Deserialização de passagem
int deserialize_passagem(const char* json_buffer, passagem_t* passagem) {
    if (!json_buffer || !passagem) return -1;
    
    // Implementação simplificada
    strcpy(passagem->tipo, "passagem");
    passagem->andar_origem = 0;
    passagem->andar_destino = 0;
    get_current_timestamp(passagem->ts, sizeof(passagem->ts));
    
    return 0;
}

// Deserialização de alerta
int deserialize_alerta(const char* json_buffer, alerta_t* alerta) {
    if (!json_buffer || !alerta) return -1;
    
    // Implementação simplificada
    strcpy(alerta->tipo, "alerta");
    strcpy(alerta->mensagem, "Alerta desconhecido");
    strcpy(alerta->placa, "UNKNOWN");
    get_current_timestamp(alerta->ts, sizeof(alerta->ts));
    
    return 0;
}