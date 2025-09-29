#include "json_utils.h"
#include <sys/socket.h>
#include <unistd.h>

// Função auxiliar para obter timestamp atual no formato ISO 8601
void get_current_timestamp(char* timestamp_buffer, size_t buffer_size) {
    time_t now;
    struct tm* tm_info;
    
    time(&now);
    tm_info = gmtime(&now);
    
    strftime(timestamp_buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}

// Serializa evento de entrada OK para JSON
int serialize_entrada_ok(const entrada_ok_t* entrada, char* json_buffer, size_t buffer_size) {
    return snprintf(json_buffer, buffer_size,
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
        entrada->andar
    );
}

// Serializa evento de saída OK para JSON
int serialize_saida_ok(const saida_ok_t* saida, char* json_buffer, size_t buffer_size) {
    return snprintf(json_buffer, buffer_size,
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
        saida->andar
    );
}

// Serializa status de vagas para JSON
int serialize_vaga_status(const vaga_status_t* status, char* json_buffer, size_t buffer_size) {
    return snprintf(json_buffer, buffer_size,
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
        status->flags.bloq2 ? "true" : "false"
    );
}

// Serializa evento de passagem para JSON
int serialize_passagem(const passagem_t* passagem, char* json_buffer, size_t buffer_size) {
    return snprintf(json_buffer, buffer_size,
        "{\n"
        "  \"tipo\": \"%s\",\n"
        "  \"andar_origem\": %d,\n"
        "  \"andar_destino\": %d,\n"
        "  \"ts\": \"%s\"\n"
        "}",
        passagem->tipo,
        passagem->andar_origem,
        passagem->andar_destino,
        passagem->ts
    );
}

// Serializa evento de alerta para JSON
int serialize_alerta(const alerta_t* alerta, char* json_buffer, size_t buffer_size) {
    return snprintf(json_buffer, buffer_size,
        "{\n"
        "  \"tipo\": \"%s\",\n"
        "  \"mensagem\": \"%s\",\n"
        "  \"placa\": \"%s\",\n"
        "  \"ts\": \"%s\"\n"
        "}",
        alerta->tipo,
        alerta->mensagem,
        alerta->placa,
        alerta->ts
    );
}

// Função auxiliar para encontrar valor em JSON simples
static char* find_json_value(const char* json, const char* key) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    
    char* key_pos = strstr(json, search_key);
    if (!key_pos) return NULL;
    
    char* colon_pos = strchr(key_pos, ':');
    if (!colon_pos) return NULL;
    
    char* value_start = colon_pos + 1;
    while (*value_start == ' ' || *value_start == '\t') value_start++;
    
    if (*value_start == '"') {
        value_start++; // Pular aspas de abertura
    }
    
    return value_start;
}

// Função auxiliar para extrair string entre aspas
static int extract_quoted_string(const char* start, char* output, size_t max_len) {
    if (*start != '"') return 0;
    
    const char* end = strchr(start + 1, '"');
    if (!end) return 0;
    
    size_t len = end - start - 1;
    if (len >= max_len) len = max_len - 1;
    
    strncpy(output, start + 1, len);
    output[len] = '\0';
    return 1;
}

// Deserializa evento de entrada OK do JSON
int deserialize_entrada_ok(const char* json_buffer, entrada_ok_t* entrada) {
    char* tipo = find_json_value(json_buffer, "tipo");
    char* placa = find_json_value(json_buffer, "placa");
    char* conf = find_json_value(json_buffer, "conf");
    char* ts = find_json_value(json_buffer, "ts");
    char* andar = find_json_value(json_buffer, "andar");
    
    if (!tipo || !placa || !conf || !ts || !andar) return -1;
    
    extract_quoted_string(tipo, entrada->tipo, sizeof(entrada->tipo));
    extract_quoted_string(placa, entrada->placa, sizeof(entrada->placa));
    extract_quoted_string(ts, entrada->ts, sizeof(entrada->ts));
    
    entrada->conf = atoi(conf);
    entrada->andar = atoi(andar);
    
    return 0;
}

// Deserializa evento de saída OK do JSON
int deserialize_saida_ok(const char* json_buffer, saida_ok_t* saida) {
    char* tipo = find_json_value(json_buffer, "tipo");
    char* placa = find_json_value(json_buffer, "placa");
    char* conf = find_json_value(json_buffer, "conf");
    char* ts = find_json_value(json_buffer, "ts");
    char* andar = find_json_value(json_buffer, "andar");
    
    if (!tipo || !placa || !conf || !ts || !andar) return -1;
    
    extract_quoted_string(tipo, saida->tipo, sizeof(saida->tipo));
    extract_quoted_string(placa, saida->placa, sizeof(saida->placa));
    extract_quoted_string(ts, saida->ts, sizeof(saida->ts));
    
    saida->conf = atoi(conf);
    saida->andar = atoi(andar);
    
    return 0;
}

// Deserializa status de vagas do JSON
int deserialize_vaga_status(const char* json_buffer, vaga_status_t* status) {
    char* tipo = find_json_value(json_buffer, "tipo");
    char* livres_a1 = find_json_value(json_buffer, "livres_a1");
    char* livres_a2 = find_json_value(json_buffer, "livres_a2");
    char* livres_total = find_json_value(json_buffer, "livres_total");
    char* lotado = find_json_value(json_buffer, "lotado");
    char* bloq2 = find_json_value(json_buffer, "bloq2");
    
    if (!tipo || !livres_a1 || !livres_a2 || !livres_total || !lotado || !bloq2) return -1;
    
    extract_quoted_string(tipo, status->tipo, sizeof(status->tipo));
    
    status->livres_a1 = atoi(livres_a1);
    status->livres_a2 = atoi(livres_a2);
    status->livres_total = atoi(livres_total);
    status->flags.lotado = (strstr(lotado, "true") != NULL);
    status->flags.bloq2 = (strstr(bloq2, "true") != NULL);
    
    return 0;
}

// Deserializa evento de passagem do JSON
int deserialize_passagem(const char* json_buffer, passagem_t* passagem) {
    char* tipo = find_json_value(json_buffer, "tipo");
    char* andar_origem = find_json_value(json_buffer, "andar_origem");
    char* andar_destino = find_json_value(json_buffer, "andar_destino");
    char* ts = find_json_value(json_buffer, "ts");
    
    if (!tipo || !andar_origem || !andar_destino || !ts) return -1;
    
    extract_quoted_string(tipo, passagem->tipo, sizeof(passagem->tipo));
    extract_quoted_string(ts, passagem->ts, sizeof(passagem->ts));
    
    passagem->andar_origem = atoi(andar_origem);
    passagem->andar_destino = atoi(andar_destino);
    
    return 0;
}

// Deserializa evento de alerta do JSON
int deserialize_alerta(const char* json_buffer, alerta_t* alerta) {
    char* tipo = find_json_value(json_buffer, "tipo");
    char* mensagem = find_json_value(json_buffer, "mensagem");
    char* placa = find_json_value(json_buffer, "placa");
    char* ts = find_json_value(json_buffer, "ts");
    
    if (!tipo || !mensagem || !placa || !ts) return -1;
    
    extract_quoted_string(tipo, alerta->tipo, sizeof(alerta->tipo));
    extract_quoted_string(mensagem, alerta->mensagem, sizeof(alerta->mensagem));
    extract_quoted_string(placa, alerta->placa, sizeof(alerta->placa));
    extract_quoted_string(ts, alerta->ts, sizeof(alerta->ts));
    
    return 0;
}

// Envia mensagem JSON via socket
int send_json_message(int socket, const char* json_message) {
    size_t message_len = strlen(json_message);
    
    // Enviar tamanho da mensagem primeiro
    if (send(socket, &message_len, sizeof(message_len), 0) < 0) {
        return -1;
    }
    
    // Enviar mensagem JSON
    if (send(socket, json_message, message_len, 0) < 0) {
        return -1;
    }
    
    return 0;
}

// Recebe mensagem JSON via socket
int receive_json_message(int socket, char* json_buffer, size_t buffer_size) {
    size_t message_len;
    
    // Receber tamanho da mensagem primeiro
    if (recv(socket, &message_len, sizeof(message_len), 0) < 0) {
        return -1;
    }
    
    // Verificar se o buffer é grande o suficiente
    if (message_len >= buffer_size) {
        return -1;
    }
    
    // Receber mensagem JSON
    if (recv(socket, json_buffer, message_len, 0) < 0) {
        return -1;
    }
    
    json_buffer[message_len] = '\0';
    return 0;
}
