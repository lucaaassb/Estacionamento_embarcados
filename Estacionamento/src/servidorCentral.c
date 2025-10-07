#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <bcm2835.h>
#include <pthread.h>
#include <stdbool.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "modbus_utils.h"
#include "common_utils.h"
#include "json_utils.h"

#define TAMANHO_VETOR_RECEBER 23
#define TAMANHO_VETOR_ENVIAR 5
static int dados_terreo[TAMANHO_VETOR_RECEBER];
static int dados_andar1[TAMANHO_VETOR_RECEBER];
static int dados_andar2[TAMANHO_VETOR_RECEBER];
static int comandos_enviar[TAMANHO_VETOR_ENVIAR];
int r = 0;
int manual =  0;

// Protótipos
void mostrar_tickets_temporarios();
void mostrar_alertas_auditoria();


/*
parametros[0] = vagas disponiveis pcd;       parametros[10] = v[7].ocupado;
parametros[1] = vagas disponiveis idoso;     parametros[11] = bool carro entrando;
parametros[2] = vagas disponiveis regular;   parametros[12] = numero carro entrando ;
parametros[3] = v[0].ocupado;                parametros[13] = vaga estacionada carro entrando;
parametros[4] = v[1].ocupado;                parametros[14] = bool carro saindo;
parametros[4] = v[2].ocupado;                parametros[15] = numero do carro saindo;
parametros[5] = v[3].ocupado;                parametros[16] = tempo de permanencia do carro saindo;
parametros[6] = v[4].ocupado;                parametros[17] = numero da vaga do carro saindo;
parametros[7] = v[5].ocupado;                parametros[18] = quantidade de vagas ocupadas;
parametros[8] = v[6].ocupado;                parametros[19] = carro em transito andares
                                             parametros[20] = recebe sinal lotado andar;
                                             parametros[21] = carro em transito terreo
                                            */

int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;
 
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
 
  ch = getchar();
 
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);
 
  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }
 
  return 0;
}




void menu(pthread_t fRecebeTerreo, pthread_t fRecebePrimeiroAndar, pthread_t fRecebeSegundoAndar){
    // Variáveis para controle de timing
    static time_t last_update = 0;
    static bool needs_clear = true;
    
    while(1){
        time_t current_time = time(NULL);
        
        // Só limpa a tela se necessário e com intervalo mínimo
        if (needs_clear && (current_time - last_update >= 1)) {
            system("clear");
            needs_clear = false;
            last_update = current_time;
        }
        
        printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
        printf("║                        SISTEMA DE CONTROLE DE ESTACIONAMENTO                 ║\n");
        printf("╚══════════════════════════════════════════════════════════════════════════════╝\n\n");
        
        printf("  📍 MAPA DE VAGAS OCUPADAS:\n");
        printf("                  │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │\n");
        printf("                   ────────────────────────────────\n");        
        printf("      2º Andar: B │ %d │ %d │ %d │ %d │ %d │ %d │ %d │ %d │\n", 
               dados_andar2[3], dados_andar2[4], dados_andar2[5], dados_andar2[6], 
               dados_andar2[7], dados_andar2[8], dados_andar2[9], dados_andar2[10]);
        printf("                   ────────────────────────────────\n");   
        printf("      1º Andar: A │ %d │ %d │ %d │ %d │ %d │ %d │ %d │ %d │\n", 
               dados_andar1[3], dados_andar1[4], dados_andar1[5], dados_andar1[6], 
               dados_andar1[7], dados_andar1[8], dados_andar1[9], dados_andar1[10]);
        printf("                   ────────────────────────────────\n");   
        printf("      Térreo:   T │ %d │ %d │ %d │ %d │ - │ - │ - │ - │\n", 
               dados_terreo[3], dados_terreo[4], dados_terreo[5], dados_terreo[6]);
        printf("                   ────────────────────────────────\n\n"); 

        printf("  📊 VAGAS DISPONÍVEIS:\n");
        printf("                  │ PcD │ Idoso │ Regular │ Total │\n");
        printf("                   ────────────────────────────────\n"); 
        printf("      2º Andar:   │  %d  │   %d   │    %d    │   %d   │\n", 
               dados_andar2[0], dados_andar2[1], dados_andar2[2], 
               dados_andar2[0]+dados_andar2[1]+dados_andar2[2]);
        printf("                   ────────────────────────────────\n"); 
        printf("      1º Andar:   │  %d  │   %d   │    %d    │   %d   │\n", 
               dados_andar1[0], dados_andar1[1], dados_andar1[2], 
               dados_andar1[0]+dados_andar1[1]+dados_andar1[2]);
        printf("                   ────────────────────────────────\n"); 
        printf("      Térreo:     │  %d  │   %d   │    %d    │   %d   │\n", 
               dados_terreo[0], dados_terreo[1], dados_terreo[2], 
               dados_terreo[0]+dados_terreo[1]+dados_terreo[2]);
        printf("                   ────────────────────────────────\n\n"); 
        
        printf("  🚗 CARROS NO ESTACIONAMENTO:\n");
        printf("                  │ Total │ Térreo │ 1º andar │ 2º andar │\n");
        printf("                  │   %d   │    %d   │     %d    │     %d    │\n", 
               dados_terreo[18]+dados_andar1[18]+dados_andar2[18], 
               dados_terreo[18], dados_andar1[18], dados_andar2[18]);
        printf("                   ──────────────────────────────────────\n\n");
        
        // Status do sistema
        if(comandos_enviar[1] == 1){
            printf("  🚫 ESTACIONAMENTO FECHADO\n");
        }
        if(dados_andar1[20] == 1){
            printf("  🚫 1º ANDAR FECHADO\n");
        }
        if(dados_andar2[20] == 1){
            printf("  🚫 2º ANDAR FECHADO\n");
        }
        
        // Eventos recentes (sem delay para evitar travamento)
        if(dados_andar1[14]==1){
            float valor = (dados_andar1[16]) * 0.15f;
            printf("  💰 Carro %d saiu da vaga A%d - Pagou R$ %.2f\n", 
                   dados_andar1[15], dados_andar1[17], valor);
            // Reset flag após exibir
            dados_andar1[14] = 0;
            needs_clear = true;
        }
        if(dados_andar2[14]==1){
            float valor = (dados_andar2[16]) * 0.15f;
            printf("  💰 Carro %d saiu da vaga B%d - Pagou R$ %.2f\n", 
                   dados_andar2[15], dados_andar2[17], valor);
            // Reset flag após exibir
            dados_andar2[14] = 0;
            needs_clear = true;
        }   
        if(dados_terreo[14]==1){
            float valor = (dados_terreo[16]) * 0.15f;
            printf("  💰 Carro %d saiu da vaga T%d - Pagou R$ %.2f\n", 
                   dados_terreo[15], dados_terreo[17], valor);
            // Reset flag após exibir
            dados_terreo[14] = 0;
            needs_clear = true;
        }
        if(dados_andar1[11]==1){
            printf("  🚗 Carro %d entrou na vaga A%d\n", dados_andar1[12], dados_andar1[13]);
            // Reset flag após exibir
            dados_andar1[11] = 0;
            needs_clear = true;
        }
        if(dados_andar2[11]==1){
            printf("  🚗 Carro %d entrou na vaga B%d\n", dados_andar2[12], dados_andar2[13]);
            // Reset flag após exibir
            dados_andar2[11] = 0;
            needs_clear = true;
        }
        if(dados_terreo[11]==1){
            printf("  🚗 Carro %d entrou na vaga T%d\n", dados_terreo[12], dados_terreo[13]);
            // Reset flag após exibir
            dados_terreo[11] = 0;
            needs_clear = true;
        }

        printf("\n  ⚙️  COMANDOS:\n");
        printf("  1 - Abrir estacionamento\n");
        printf("  2 - Fechar estacionamento\n");
        printf("  3 - Ativar 1º andar\n");
        printf("  4 - Desativar 1º andar\n");
        printf("  5 - Ativar 2º andar\n");
        printf("  6 - Desativar 2º andar\n");
        printf("  7 - Ver tickets temporários\n");
        printf("  8 - Ver alertas de auditoria\n");
        printf("  q - Encerrar estacionamento\n\n");      

        // Lógica automática de fechamento
        if((dados_terreo[18] == 4 && r==0 && (dados_andar1[18]== 8 || dados_andar1[20] == 1) && (dados_andar2[18] == 8 || dados_andar2[20] == 1))){
            comandos_enviar[1] = 1;
            r = 1;
        } 
        else if(((dados_terreo[18] < 4 && r == 1) || dados_andar1[20] == 0 || dados_andar2[20] == 0) && manual == 0){
            comandos_enviar[1] = 0;          
            r = 0;
        }

        if(kbhit()){
            char opcao = getchar();
            
            switch(opcao)
            {
            case '1':
                comandos_enviar[1] = 0;
                r = 0;
                manual = 0;
                log_info("Estacionamento aberto manualmente");
                break;
            case '2':
                comandos_enviar[1] = 1;
                r = 1;
                manual = 1;
                log_info("Estacionamento fechado manualmente");
                break;
            case '3':
                comandos_enviar[2] = 0;
                log_info("1º andar ativado");
                break;
            case '4':
                comandos_enviar[2] = 1;
                log_info("1º andar desativado");
                break;
            case '5':
                comandos_enviar[3] = 0;
                log_info("2º andar ativado");
                break;
            case '6':
                comandos_enviar[3] = 1;
                log_info("2º andar desativado");
                break;
            case '7':
                mostrar_tickets_temporarios();
                break;
            case '8':
                mostrar_alertas_auditoria();
                break;
            case 'q':
                log_info("Encerrando sistema...");
                pthread_cancel(fRecebeTerreo);
                pthread_cancel(fRecebePrimeiroAndar);
                pthread_cancel(fRecebeSegundoAndar);
                exit(0);
            default:
                break;
            }
        }
        printf("\n");

        // Delay mais suave para evitar piscar excessivo
        usleep(100000); // 100ms em vez de 1000ms
    }  
}

    
void *recebePrimeiroAndar(){
    char *ip ="0.0.0.0";  // Escutar em todas as interfaces
    int port = 10681;

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    int n;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0){
        perror("[-]Socket error");
        exit(1);
    }
    printf("[+]Server socket created\n");

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    n = bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(n < 0){
        perror("[-]Bind error");
        exit(1);
    }
    printf("[+]Bind to port %d\n", port);

    listen(server_sock, 5);
    
    printf("[+]Listening...\n");

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
    printf("Client Connected\n");
    
    char json_buffer[1024];
    vaga_status_t status_resposta;
    
    while(1){
        // Receber mensagem JSON do 1º andar
        if (receive_json_message(client_sock, json_buffer, sizeof(json_buffer)) == 0) {
            printf("Recebido do 1º andar: %s\n", json_buffer);
            
            // Processar mensagem recebida (pode ser entrada, saída, passagem, etc.)
            // Por enquanto, manter compatibilidade com arrays antigos
            recv(client_sock, dados_andar1, TAMANHO_VETOR_RECEBER * sizeof(int), 0);
        }
        
        // Enviar resposta JSON
        strcpy(status_resposta.tipo, "vaga_status");
        status_resposta.livres_a1 = dados_andar1[0] + dados_andar1[1] + dados_andar1[2];
        status_resposta.livres_a2 = dados_andar2[0] + dados_andar2[1] + dados_andar2[2];
        status_resposta.livres_total = status_resposta.livres_a1 + status_resposta.livres_a2;
        status_resposta.flags.lotado = (dados_andar1[20] == 1 || dados_andar2[20] == 1);
        status_resposta.flags.bloq2 = (dados_andar2[20] == 1);
        
        char response_json[512];
        serialize_vaga_status(&status_resposta, response_json, sizeof(response_json));
        send_json_message(client_sock, response_json);
        // Enviar comandos para o 1º andar
        send(client_sock, comandos_enviar, TAMANHO_VETOR_ENVIAR * sizeof(int), 0);
        
        delay(1000);
    }
    close(client_sock);
    printf("Client Disconnected\n");
}

void *recebeSegundoAndar(){
    char *ip ="0.0.0.0";  // Escutar em todas as interfaces
    int port = 10682;

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    int n;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0){
        perror("[-]Socket error");
        exit(1);
    }
    printf("[+]Server socket created\n");

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    n = bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(n < 0){
        perror("[-]Bind error");
        exit(1);
    }
    printf("[+]Bind to port %d\n", port);

    listen(server_sock, 5);
    
    printf("[+]Listening...\n");

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
    printf("Client Connected\n");
    
    char json_buffer[1024];
    vaga_status_t status_resposta;
    
    while(1){
        // Receber mensagem JSON do 2º andar
        if (receive_json_message(client_sock, json_buffer, sizeof(json_buffer)) == 0) {
            printf("Recebido do 2º andar: %s\n", json_buffer);
            
            // Processar mensagem recebida (pode ser entrada, saída, passagem, etc.)
            // Por enquanto, manter compatibilidade com arrays antigos
            recv(client_sock, dados_andar2, TAMANHO_VETOR_RECEBER * sizeof(int), 0);
        }
        
        // Enviar resposta JSON
        strcpy(status_resposta.tipo, "vaga_status");
        status_resposta.livres_a1 = dados_andar1[0] + dados_andar1[1] + dados_andar1[2];
        status_resposta.livres_a2 = dados_andar2[0] + dados_andar2[1] + dados_andar2[2];
        status_resposta.livres_total = status_resposta.livres_a1 + status_resposta.livres_a2;
        status_resposta.flags.lotado = (dados_andar1[20] == 1 || dados_andar2[20] == 1);
        status_resposta.flags.bloq2 = (dados_andar2[20] == 1);
        
        char response_json[512];
        serialize_vaga_status(&status_resposta, response_json, sizeof(response_json));
        send_json_message(client_sock, response_json);
        // Enviar comandos para o 2º andar
        send(client_sock, comandos_enviar, TAMANHO_VETOR_ENVIAR * sizeof(int), 0);
        
        delay(1000);
    }
    close(client_sock);
    printf("Client 2 Disconnected\n");
}

void *recebeTerreo(){
    char *ip ="0.0.0.0";  // Escutar em todas as interfaces
    int port = 10683;

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    
    int n;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0){
        perror("[-]Socket error");
        exit(1);
    }
    printf("[+]Server socket created\n");

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    n = bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(n < 0){
        perror("[-]Bind error");
        exit(1);
    }
    printf("[+]Bind to port %d\n", port);

    listen(server_sock, 5);
    
    printf("[+]Listening...\n");

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
    printf("Client 3 Connected\n");
    
    char json_buffer[1024];
    vaga_status_t status_resposta;
    
    while(1){
        // Receber mensagem JSON do térreo
        if (receive_json_message(client_sock, json_buffer, sizeof(json_buffer)) == 0) {
            printf("Recebido do térreo: %s\n", json_buffer);
            
            // Processar mensagem recebida (pode ser entrada, saída, passagem, etc.)
            // Por enquanto, manter compatibilidade com arrays antigos
            recv(client_sock, dados_terreo, TAMANHO_VETOR_RECEBER * sizeof(int), 0);
        }
        
        // Enviar resposta JSON
        strcpy(status_resposta.tipo, "vaga_status");
        status_resposta.livres_a1 = dados_andar1[0] + dados_andar1[1] + dados_andar1[2];
        status_resposta.livres_a2 = dados_andar2[0] + dados_andar2[1] + dados_andar2[2];
        status_resposta.livres_total = status_resposta.livres_a1 + status_resposta.livres_a2;
        status_resposta.flags.lotado = (dados_andar1[20] == 1 || dados_andar2[20] == 1);
        status_resposta.flags.bloq2 = (dados_andar2[20] == 1);
        
        char response_json[512];
        serialize_vaga_status(&status_resposta, response_json, sizeof(response_json));
        send_json_message(client_sock, response_json);
        
        // Opcional: exemplo de atualização de algum comando baseado no térreo
        comandos_enviar[0] = dados_terreo[12];
        // Enviar comandos para o térreo
        send(client_sock, comandos_enviar, TAMANHO_VETOR_ENVIAR * sizeof(int), 0);
        delay(1000);
    }
    close(client_sock);
    printf("Client Disconnected\n");
}

// Função para mostrar tickets temporários
void mostrar_tickets_temporarios() {
    system("clear");
    printf("=== TICKETS TEMPORÁRIOS ===\n\n");
    
    ticket_temporario_t tickets[50];
    int count = listar_tickets_ativos(tickets, 50);
    
    if (count == 0) {
        printf("Nenhum ticket temporário ativo.\n");
    } else {
        printf("ID\tPlaca\t\tConfiança\tVaga\tAndar\tTimestamp\n");
        printf("--------------------------------------------------------\n");
        for (int i = 0; i < count; i++) {
            printf("%d\t%s\t\t%d%%\t\t%d\t%d\t%s", 
                   tickets[i].ticket_id,
                   tickets[i].placa_temporaria,
                   tickets[i].confianca,
                   tickets[i].vaga_associada,
                   tickets[i].andar,
                   ctime(&tickets[i].timestamp));
        }
    }
    
    printf("\nPressione Enter para continuar...");
    getchar();
}

// Função para mostrar alertas de auditoria
void mostrar_alertas_auditoria() {
    system("clear");
    printf("=== ALERTAS DE AUDITORIA ===\n\n");
    
    alerta_auditoria_t alertas[50];
    listar_alertas_pendentes(alertas, 50);
    
    int count = 0;
    for (int i = 0; i < 50; i++) {
        if (alertas[i].timestamp > 0) count++;
    }
    
    if (count == 0) {
        printf("Nenhum alerta de auditoria pendente.\n");
    } else {
        printf("Placa\t\tMotivo\t\t\t\tTipo\tTimestamp\n");
        printf("--------------------------------------------------------\n");
        for (int i = 0; i < count; i++) {
            printf("%s\t\t%s\t\t%d\t%s", 
                   alertas[i].placa_veiculo,
                   alertas[i].motivo,
                   alertas[i].tipo_alerta,
                   ctime(&alertas[i].timestamp));
        }
    }
    
    printf("\nPressione Enter para continuar...");
    getchar();
}

int mainC(){
    // Inicializar sistema de logs
    init_log_system();
    log_info("Iniciando servidor central");
    
    pthread_t fRecebePrimeiroAndar, fRecebeSegundoAndar, fRecebeTerreo;
    
    log_info("Criando threads do servidor central");
    pthread_create(&fRecebePrimeiroAndar, NULL, recebePrimeiroAndar, NULL);
    pthread_create(&fRecebeSegundoAndar, NULL, recebeSegundoAndar, NULL);
    pthread_create(&fRecebeTerreo, NULL, recebeTerreo, NULL);
    
    log_info("Servidor central em execução");
    menu(fRecebeTerreo, fRecebePrimeiroAndar, fRecebeSegundoAndar);
    
    pthread_join(fRecebePrimeiroAndar, NULL);
    pthread_join(fRecebeSegundoAndar, NULL);
    pthread_join(fRecebeTerreo, NULL);
    
    close_log_system();
    log_info("Servidor central finalizado");
    return 0;
}