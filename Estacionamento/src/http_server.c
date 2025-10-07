#include "http_server.h"
#include "common_utils.h"

// Instância global dos dados do estacionamento
static estacionamento_data_t estacionamento_data;
static int http_server_running = 0;

void update_estacionamento_data(estacionamento_data_t* data, int* terreo, int* andar1, int* andar2, int* comandos) {
    pthread_mutex_lock(&data->mutex);
    
    for(int i = 0; i < 23; i++) {
        data->dados_terreo[i] = terreo[i];
        data->dados_andar1[i] = andar1[i];
        data->dados_andar2[i] = andar2[i];
    }
    
    for(int i = 0; i < 5; i++) {
        data->comandos_enviar[i] = comandos[i];
    }
    
    pthread_mutex_unlock(&data->mutex);
}

void send_http_response(int client_sock, int status_code, const char* content_type, const char* body) {
    char response[4096];
    int body_len = strlen(body);
    
    snprintf(response, sizeof(response),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status_code, content_type, body_len, body);
    
    send(client_sock, response, strlen(response), 0);
}

void handle_status_request(int client_sock, estacionamento_data_t* data) {
    char json_response[2048];
    
    pthread_mutex_lock(&data->mutex);
    
    snprintf(json_response, sizeof(json_response),
        "{\n"
        "  \"terreo\": {\n"
        "    \"vagas_pcd\": %d,\n"
        "    \"vagas_idoso\": %d,\n"
        "    \"vagas_regular\": %d,\n"
        "    \"total_ocupadas\": %d,\n"
        "    \"carro_entrando\": %s,\n"
        "    \"carro_saindo\": %s,\n"
        "    \"numero_carro_entrando\": %d,\n"
        "    \"vaga_carro_entrando\": %d,\n"
        "    \"numero_carro_saindo\": %d,\n"
        "    \"tempo_permanencia\": %d,\n"
        "    \"vaga_carro_saindo\": %d\n"
        "  },\n"
        "  \"andar1\": {\n"
        "    \"vagas_pcd\": %d,\n"
        "    \"vagas_idoso\": %d,\n"
        "    \"vagas_regular\": %d,\n"
        "    \"total_ocupadas\": %d,\n"
        "    \"carro_entrando\": %s,\n"
        "    \"carro_saindo\": %s,\n"
        "    \"lotado\": %s\n"
        "  },\n"
        "  \"andar2\": {\n"
        "    \"vagas_pcd\": %d,\n"
        "    \"vagas_idoso\": %d,\n"
        "    \"vagas_regular\": %d,\n"
        "    \"total_ocupadas\": %d,\n"
        "    \"carro_entrando\": %s,\n"
        "    \"carro_saindo\": %s,\n"
        "    \"lotado\": %s\n"
        "  },\n"
        "  \"sistema\": {\n"
        "    \"estacionamento_fechado\": %s,\n"
        "    \"andar1_desativado\": %s,\n"
        "    \"andar2_desativado\": %s\n"
        "  }\n"
        "}",
        // Térreo
        data->dados_terreo[0], data->dados_terreo[1], data->dados_terreo[2],
        data->dados_terreo[18],
        data->dados_terreo[11] == 1 ? "true" : "false",
        data->dados_terreo[14] == 1 ? "true" : "false",
        data->dados_terreo[12], data->dados_terreo[13],
        data->dados_terreo[15], data->dados_terreo[16], data->dados_terreo[17],
        // 1º Andar
        data->dados_andar1[0], data->dados_andar1[1], data->dados_andar1[2],
        data->dados_andar1[18],
        data->dados_andar1[11] == 1 ? "true" : "false",
        data->dados_andar1[14] == 1 ? "true" : "false",
        data->dados_andar1[20] == 1 ? "true" : "false",
        // 2º Andar
        data->dados_andar2[0], data->dados_andar2[1], data->dados_andar2[2],
        data->dados_andar2[18],
        data->dados_andar2[11] == 1 ? "true" : "false",
        data->dados_andar2[14] == 1 ? "true" : "false",
        data->dados_andar2[20] == 1 ? "true" : "false",
        // Sistema
        data->comandos_enviar[1] == 1 ? "true" : "false",
        data->comandos_enviar[2] == 1 ? "true" : "false",
        data->comandos_enviar[3] == 1 ? "true" : "false"
    );
    
    pthread_mutex_unlock(&data->mutex);
    
    send_http_response(client_sock, 200, "application/json", json_response);
    log_info("Status enviado para dashboard");
}

void handle_entrada_request(int client_sock, estacionamento_data_t* data) {
    pthread_mutex_lock(&data->mutex);
    
    // Simular entrada de carro no térreo
    data->dados_terreo[11] = 1;  // Carro entrando
    data->dados_terreo[12] = 123;  // Número do carro
    data->dados_terreo[13] = 1;  // Vaga
    data->dados_terreo[18]++;  // Incrementar total
    
    pthread_mutex_unlock(&data->mutex);
    
    send_http_response(client_sock, 200, "application/json", 
        "{\"status\": \"success\", \"message\": \"Carro adicionado\"}");
    log_info("Entrada de carro simulada");
}

void handle_saida_request(int client_sock, estacionamento_data_t* data) {
    pthread_mutex_lock(&data->mutex);
    
    // Simular saída de carro do térreo
    data->dados_terreo[14] = 1;  // Carro saindo
    data->dados_terreo[15] = 123;  // Número do carro
    data->dados_terreo[16] = 120;  // Tempo em minutos
    data->dados_terreo[17] = 1;  // Vaga
    if(data->dados_terreo[18] > 0) {
        data->dados_terreo[18]--;  // Decrementar total
    }
    
    pthread_mutex_unlock(&data->mutex);
    
    send_http_response(client_sock, 200, "application/json", 
        "{\"status\": \"success\", \"message\": \"Carro removido\"}");
    log_info("Saída de carro simulada");
}

void parse_http_request(const char* request, char* method, char* path) {
    sscanf(request, "%s %s", method, path);
}

void* http_server_thread(void* arg) {
    int port = *(int*)arg;
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char request[1024];
    char method[16], path[256];
    
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0) {
        perror("[-]HTTP Server Socket error");
        return NULL;
    }
    
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if(bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-]HTTP Server Bind error");
        close(server_sock);
        return NULL;
    }
    
    if(listen(server_sock, 5) < 0) {
        perror("[-]HTTP Server Listen error");
        close(server_sock);
        return NULL;
    }
    
    log_info("Servidor HTTP iniciado na porta %d", port);
    printf("🌐 Dashboard disponível em: http://localhost:%d/status\n", port);
    printf("📋 Endpoints disponíveis:\n");
    printf("   GET  /status  - Status do estacionamento\n");
    printf("   GET  /entrada - Simular entrada de carro\n");
    printf("   GET  /saida   - Simular saída de carro\n");
    
    http_server_running = 1;
    
    while(http_server_running) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        
        if(client_sock < 0) {
            continue;
        }
        
        // Ler requisição HTTP
        int bytes_received = recv(client_sock, request, sizeof(request) - 1, 0);
        if(bytes_received > 0) {
            request[bytes_received] = '\0';
            
            // Parse da requisição
            parse_http_request(request, method, path);
            
            // Processar requisição
            if(strcmp(method, "GET") == 0) {
                if(strcmp(path, "/status") == 0) {
                    handle_status_request(client_sock, &estacionamento_data);
                } else if(strcmp(path, "/entrada") == 0) {
                    handle_entrada_request(client_sock, &estacionamento_data);
                } else if(strcmp(path, "/saida") == 0) {
                    handle_saida_request(client_sock, &estacionamento_data);
                } else {
                    send_http_response(client_sock, 404, "text/plain", "Not Found");
                }
            } else if(strcmp(method, "OPTIONS") == 0) {
                // CORS preflight
                send_http_response(client_sock, 200, "text/plain", "");
            } else {
                send_http_response(client_sock, 405, "text/plain", "Method Not Allowed");
            }
        }
        
        close(client_sock);
    }
    
    close(server_sock);
    return NULL;
}

int init_http_server(int port) {
    // Inicializar mutex
    if(pthread_mutex_init(&estacionamento_data.mutex, NULL) != 0) {
        log_erro("Falha ao inicializar mutex do servidor HTTP");
        return -1;
    }
    
    // Inicializar dados
    memset(&estacionamento_data, 0, sizeof(estacionamento_data));
    
    // Criar thread do servidor HTTP
    pthread_t http_thread;
    if(pthread_create(&http_thread, NULL, http_server_thread, &port) != 0) {
        log_erro("Falha ao criar thread do servidor HTTP");
        pthread_mutex_destroy(&estacionamento_data.mutex);
        return -1;
    }
    
    pthread_detach(http_thread);
    
    // Aguardar servidor inicializar
    usleep(500000); // 500ms
    
    return 0;
}

void stop_http_server() {
    http_server_running = 0;
    pthread_mutex_destroy(&estacionamento_data.mutex);
}
