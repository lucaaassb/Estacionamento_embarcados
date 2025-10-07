#include "thingsboard.h"
#include "common_utils.h"

static CURL *curl_handle = NULL;
static pthread_mutex_t tb_mutex = PTHREAD_MUTEX_INITIALIZER;

// Callback para capturar resposta HTTP
size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, http_response_t *response) {
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->memory, response->size + realsize + 1);
    
    if (!ptr) {
        log_erro("Erro de memória no callback HTTP");
        return 0;
    }
    
    response->memory = ptr;
    memcpy(&(response->memory[response->size]), contents, realsize);
    response->size += realsize;
    response->memory[response->size] = 0;
    
    return realsize;
}

// Inicializar cliente ThingsBoard
int init_thingsboard_client() {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        log_erro("Falha ao inicializar libcurl");
        return -1;
    }
    
    curl_handle = curl_easy_init();
    if (!curl_handle) {
        log_erro("Falha ao criar handle curl");
        curl_global_cleanup();
        return -1;
    }
    
    // Configurações comuns
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
    
    log_info("Cliente ThingsBoard inicializado");
    return 0;
}

// Limpar cliente ThingsBoard  
void cleanup_thingsboard_client() {
    if (curl_handle) {
        curl_easy_cleanup(curl_handle);
        curl_handle = NULL;
    }
    curl_global_cleanup();
    log_info("Cliente ThingsBoard finalizado");
}

// Enviar dados de telemetria genéricos
int send_telemetry_data(const char* device_token, const char* json_data) {
    if (!curl_handle || !device_token || !json_data) {
        log_erro("Parâmetros inválidos para envio de telemetria");
        return -1;
    }
    
    pthread_mutex_lock(&tb_mutex);
    
    char url[256];
    snprintf(url, sizeof(url), "%s/api/v1/%s/telemetry", TB_SERVER_URL, device_token);
    
    http_response_t response = {0};
    struct curl_slist *headers = NULL;
    
    // Configurar headers
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    // Configurar requisição
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &response);
    
    // Executar requisição
    CURLcode res = curl_easy_perform(curl_handle);
    long response_code;
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    
    // Limpar
    curl_slist_free_all(headers);
    if (response.memory) {
        free(response.memory);
    }
    
    pthread_mutex_unlock(&tb_mutex);
    
    if (res != CURLE_OK || response_code != 200) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                "Falha no envio para ThingsBoard - CURL: %s, HTTP: %ld", 
                curl_easy_strerror(res), response_code);
        log_erro(error_msg);
        return -1;
    }
    
    log_debug("Dados enviados para ThingsBoard com sucesso");
    return 0;
}

// Enviar dados de ocupação de vagas
int send_occupancy_data(const char* device_token, int andar, int* vagas_ocupadas, int num_vagas) {
    if (!device_token || !vagas_ocupadas) return -1;
    
    json_object *root = json_object_new_object();
    json_object *andar_obj = json_object_new_int(andar);
    json_object *timestamp_obj = json_object_new_int64(time(NULL) * 1000);
    
    // Adicionar dados básicos
    json_object_object_add(root, "andar", andar_obj);
    json_object_object_add(root, "ts", timestamp_obj);
    
    // Adicionar ocupação por vaga
    int total_ocupadas = 0;
    for (int i = 0; i < num_vagas; i++) {
        char vaga_key[16];
        snprintf(vaga_key, sizeof(vaga_key), "vaga_%d", i + 1);
        json_object *vaga_obj = json_object_new_int(vagas_ocupadas[i]);
        json_object_object_add(root, vaga_key, vaga_obj);
        total_ocupadas += vagas_ocupadas[i];
    }
    
    // Adicionar totais
    json_object *total_obj = json_object_new_int(total_ocupadas);
    json_object *livres_obj = json_object_new_int(num_vagas - total_ocupadas);
    json_object_object_add(root, "vagas_ocupadas", total_obj);
    json_object_object_add(root, "vagas_livres", livres_obj);
    
    const char* json_string = json_object_to_json_string(root);
    int result = send_telemetry_data(device_token, json_string);
    
    json_object_put(root);
    return result;
}

// Enviar evento de veículo (entrada/saída)
int send_vehicle_event(const char* device_token, const char* event_type, const char* placa, int vaga, float valor) {
    if (!device_token || !event_type) return -1;
    
    json_object *root = json_object_new_object();
    json_object *event_obj = json_object_new_string(event_type);
    json_object *timestamp_obj = json_object_new_int64(time(NULL) * 1000);
    
    json_object_object_add(root, "evento", event_obj);
    json_object_object_add(root, "ts", timestamp_obj);
    
    if (placa && strlen(placa) > 0) {
        json_object *placa_obj = json_object_new_string(placa);
        json_object_object_add(root, "placa", placa_obj);
    }
    
    if (vaga > 0) {
        json_object *vaga_obj = json_object_new_int(vaga);
        json_object_object_add(root, "vaga", vaga_obj);
    }
    
    if (valor > 0.0) {
        json_object *valor_obj = json_object_new_double(valor);
        json_object_object_add(root, "valor_pago", valor_obj);
    }
    
    const char* json_string = json_object_to_json_string(root);
    int result = send_telemetry_data(device_token, json_string);
    
    json_object_put(root);
    return result;
}

// Enviar dados do sensor
int send_sensor_data(const char* device_token, const char* sensor_name, int value) {
    if (!device_token || !sensor_name) return -1;
    
    json_object *root = json_object_new_object();
    json_object *sensor_obj = json_object_new_int(value);
    json_object *timestamp_obj = json_object_new_int64(time(NULL) * 1000);
    
    json_object_object_add(root, sensor_name, sensor_obj);
    json_object_object_add(root, "ts", timestamp_obj);
    
    const char* json_string = json_object_to_json_string(root);
    int result = send_telemetry_data(device_token, json_string);
    
    json_object_put(root);
    return result;
}

// Enviar dados específicos do térreo
int send_terreo_data(int* vagas_ocupadas, int carros_entrada, int carros_saida, float valor_arrecadado) {
    json_object *root = json_object_new_object();
    json_object *timestamp_obj = json_object_new_int64(time(NULL) * 1000);
    
    json_object_object_add(root, "ts", timestamp_obj);
    json_object_object_add(root, "andar", json_object_new_int(0));
    
    // Vagas individuais (Térreo tem 4 vagas)
    for (int i = 0; i < 4; i++) {
        char vaga_key[16];
        snprintf(vaga_key, sizeof(vaga_key), "vaga_T%d", i + 1);
        json_object_object_add(root, vaga_key, json_object_new_int(vagas_ocupadas[i]));
    }
    
    // Dados de movimento
    json_object_object_add(root, "carros_entrada", json_object_new_int(carros_entrada));
    json_object_object_add(root, "carros_saida", json_object_new_int(carros_saida));
    json_object_object_add(root, "valor_arrecadado", json_object_new_double(valor_arrecadado));
    
    // Totais
    int total_ocupadas = 0;
    for (int i = 0; i < 4; i++) total_ocupadas += vagas_ocupadas[i];
    json_object_object_add(root, "vagas_ocupadas_total", json_object_new_int(total_ocupadas));
    json_object_object_add(root, "vagas_livres_total", json_object_new_int(4 - total_ocupadas));
    
    const char* json_string = json_object_to_json_string(root);
    int result = send_telemetry_data(TB_ACCESS_TOKEN_TERREO, json_string);
    
    json_object_put(root);
    return result;
}

// Enviar dados específicos do 1º andar
int send_andar1_data(int* vagas_ocupadas, int passagem_subindo, int passagem_descendo) {
    json_object *root = json_object_new_object();
    json_object *timestamp_obj = json_object_new_int64(time(NULL) * 1000);
    
    json_object_object_add(root, "ts", timestamp_obj);
    json_object_object_add(root, "andar", json_object_new_int(1));
    
    // Vagas individuais (1º Andar tem 8 vagas)
    for (int i = 0; i < 8; i++) {
        char vaga_key[16];
        snprintf(vaga_key, sizeof(vaga_key), "vaga_A%d", i + 1);
        json_object_object_add(root, vaga_key, json_object_new_int(vagas_ocupadas[i]));
    }
    
    // Dados de passagem
    json_object_object_add(root, "passagem_subindo", json_object_new_int(passagem_subindo));
    json_object_object_add(root, "passagem_descendo", json_object_new_int(passagem_descendo));
    
    // Totais
    int total_ocupadas = 0;
    for (int i = 0; i < 8; i++) total_ocupadas += vagas_ocupadas[i];
    json_object_object_add(root, "vagas_ocupadas_total", json_object_new_int(total_ocupadas));
    json_object_object_add(root, "vagas_livres_total", json_object_new_int(8 - total_ocupadas));
    
    const char* json_string = json_object_to_json_string(root);
    int result = send_telemetry_data(TB_ACCESS_TOKEN_ANDAR1, json_string);
    
    json_object_put(root);
    return result;
}

// Enviar dados específicos do 2º andar
int send_andar2_data(int* vagas_ocupadas, int passagem_subindo, int passagem_descendo) {
    json_object *root = json_object_new_object();
    json_object *timestamp_obj = json_object_new_int64(time(NULL) * 1000);
    
    json_object_object_add(root, "ts", timestamp_obj);
    json_object_object_add(root, "andar", json_object_new_int(2));
    
    // Vagas individuais (2º Andar tem 8 vagas)
    for (int i = 0; i < 8; i++) {
        char vaga_key[16];
        snprintf(vaga_key, sizeof(vaga_key), "vaga_B%d", i + 1);
        json_object_object_add(root, vaga_key, json_object_new_int(vagas_ocupadas[i]));
    }
    
    // Dados de passagem
    json_object_object_add(root, "passagem_subindo", json_object_new_int(passagem_subindo));
    json_object_object_add(root, "passagem_descendo", json_object_new_int(passagem_descendo));
    
    // Totais
    int total_ocupadas = 0;
    for (int i = 0; i < 8; i++) total_ocupadas += vagas_ocupadas[i];
    json_object_object_add(root, "vagas_ocupadas_total", json_object_new_int(total_ocupadas));
    json_object_object_add(root, "vagas_livres_total", json_object_new_int(8 - total_ocupadas));
    
    const char* json_string = json_object_to_json_string(root);
    int result = send_telemetry_data(TB_ACCESS_TOKEN_ANDAR2, json_string);
    
    json_object_put(root);
    return result;
}
