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
#include "modbus_utils.h"
#include "common_utils.h"
#include "json_utils.h"

#define tamVetorReceber 23
#define tamVetorEnviar 5
int terreo[tamVetorReceber];
int andar1[tamVetorReceber];
int andar2[tamVetorReceber];
int enviar[tamVetorEnviar];
int r = 0;
int manual =  0;


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

   

    while(1){
        system("clear");
        printf("  Vagas ocupadas:\n");
        printf("                  | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |\n");
        printf("                   -------------------------------\n");        
        printf("      2º Andar: B | %d | %d | %d | %d | %d | %d | %d | %d |\n", andar2[3], andar2[4], andar2[5], andar2[6], andar2[7], andar2[8], andar2[9], andar2[10]);
        printf("                   -------------------------------\n");   
        printf("      1º Andar: A | %d | %d | %d | %d | %d | %d | %d | %d |\n", andar1[3], andar1[4], andar1[5], andar1[6], andar1[7], andar1[8], andar1[9], andar1[10]);
        printf("                   -------------------------------\n");   
        printf("      Terreo:   T | %d | %d | %d | %d | - | - | - | - |\n", terreo[3], terreo[4], terreo[5], terreo[6]);
        printf("                   -------------------------------\n"); 

        printf("\n  Vagas disponíveis no estacionamento:\n");
        printf("                  | PcD | Idoso | Regular | Total |\n");
        printf("                   -------------------------------\n"); 
        printf("      2º Andar:   |  %d  |   %d   |    %d    |   %d   |\n", andar2[0], andar2[1], andar2[2], andar2[0]+andar2[1]+andar2[2]);
        printf("                   -------------------------------\n"); 
        printf("      1º Andar:   |  %d  |   %d   |    %d    |   %d   |\n", andar1[0], andar1[1], andar1[2], andar1[0]+andar1[1]+andar1[2]);
        printf("                   -------------------------------\n"); 
        printf("      Terreo:     |  %d  |   %d   |    %d    |   %d   |\n", terreo[0], terreo[1], terreo[2], terreo[0]+terreo[1]+terreo[2]);
        printf("                   -------------------------------\n"); 
        printf("\n  Carros no estacionamento:\n");
        

        
        printf("                  | Total | Terreo | 1º andar | 2º andar | \n");
        printf("                  |   %d   |    %d   |     %d    |     %d    | \n", terreo[18]+andar1[18]+andar2[18] , terreo[18], andar1[18], andar2[18]);
        float y = (andar1[16])*0.1;
        float w = (andar2[16])*0.1;
        float z = (terreo[16])*0.1; 
        
        if(enviar[1] == 1){
            printf("\n              -----------------------------------\n");
            printf("             |      Estacionamento fechado       |\n");
            printf("              -----------------------------------\n");
        }
        if(andar1[20] == 1){
            printf("\n              -----------------------------------\n");
            printf("             |          1º andar fechado         |\n");
            printf("              -----------------------------------\n");
        }
        if(andar2[20] == 1){
            printf("\n              -----------------------------------\n");
            printf("             |          2º andar fechado         |\n");
            printf("              -----------------------------------\n");
        }
        if(andar1[14]==1){
            printf("\n              ------------------------------------\n");
            printf("             | Carro %d saiu da vaga A%d pagou %.2f |\n", andar1[15], andar1[17], y);
            printf("              ------------------------------------\n");
            delay(1500);
        }
        if(andar2[14]==1){
            printf("\n              ------------------------------------\n");
            printf("             | Carro %d saiu da vaga B%d pagou %.2f |\n", andar2[15], andar2[17], w);
            printf("              ------------------------------------\n");
            delay(1500);
        }   
        if(terreo[14]==1){
            printf("\n              ------------------------------------\n");
            printf("             | Carro %d saiu da vaga T%d pagou %.2f |\n", terreo[15], terreo[17], z);
            printf("              ------------------------------------\n");
            delay(1500);
        }
        if(andar1[11]==1){
            printf("\n                       ---------------------------\n");
            printf("                      | Carro %d entrou na vaga A%d |\n", andar1[12], andar1[13]);
            printf("                       ---------------------------\n");
            delay(1500);
        }
        if(andar2[11]==1){
            printf("\n                       ---------------------------\n");
            printf("                      | Carro %d entrou na vaga B%d |\n", andar2[12], andar2[13]);
            printf("                       ---------------------------\n");
            delay(1500);
        }
        if(terreo[11]==1){
            printf("\n                       ---------------------------\n");
            printf("                      | Carro %d entrou na vaga T%d |\n", terreo[12], terreo[13]);
            printf("                       ---------------------------\n");
            delay(1500);
        }

        printf("\n  Opções:\n");
        printf("  1 - Abrir estacionamento\n");
        printf("  2 - Fechar estacionamento\n");
        printf("  3 - Ativar 1 andar\n");
        printf("  4 - Desativar 1 andar\n");
        printf("  5 - Ativar 2 andar\n");
        printf("  6 - Desativar 2 andar\n");
        printf("  7 - Ver tickets temporários\n");
        printf("  8 - Ver alertas de auditoria\n");
        printf("  q - Encerrar estacionamento\n\n");      

        
        if((terreo[18] == 8 && r==0 &&(andar1[18]== 8 || andar1[20] == 1) && (andar2[18] == 8 || andar2[20] == 1))){
        enviar[1] = 1;
        r = 1;
        } 
        else if((terreo[18] < 8 && r == 1 || andar1[20] == 0 || andar2[20] == 0)&& manual == 0){
        enviar[1] = 0;          
        r = 0;
        }
        

        if(kbhit()){
            char opcao = getchar();
            
            switch(opcao)
            {
            case '1':
                enviar[1] = 0;
                r =0;
                manual=0;
                break;
            case '2':
                enviar[1] = 1;
                r = 1;
                manual = 1;
                break;
            case '3':
                enviar[2] = 0;
                break;
            case '4':
                enviar[2] = 1;
                break;
            case'5':
                enviar[3] = 0;
                break;
            case'6':
                enviar[3] = 1;
                break;
            case '7':
                mostrar_tickets_temporarios();
                break;
            case '8':
                mostrar_alertas_auditoria();
                break;
            case 'q':
                pthread_cancel(fRecebeTerreo);
                pthread_cancel(fRecebePrimeiroAndar);
                pthread_cancel(fRecebeSegundoAndar);
                exit(0);
            default:
                break;
            }
            
        }
        printf("\n");

        delay(1000);
    }  
}

    
void *recebePrimeiroAndar(){
    char *ip ="127.0.0.1";
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
            recv(client_sock, andar1, tamVetorReceber * sizeof(int), 0);
        }
        
        // Enviar resposta JSON
        strcpy(status_resposta.tipo, "vaga_status");
        status_resposta.livres_a1 = andar1[0] + andar1[1] + andar1[2];
        status_resposta.livres_a2 = andar2[0] + andar2[1] + andar2[2];
        status_resposta.livres_total = status_resposta.livres_a1 + status_resposta.livres_a2;
        status_resposta.flags.lotado = (andar1[20] == 1 || andar2[20] == 1);
        status_resposta.flags.bloq2 = (andar2[20] == 1);
        
        char response_json[512];
        serialize_vaga_status(&status_resposta, response_json, sizeof(response_json));
        send_json_message(client_sock, response_json);
        
        delay(1000);
    }
    close(client_sock);
    printf("Client Disconnected\n");
}

void *recebeSegundoAndar(){
    char *ip ="127.0.0.1";
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
            recv(client_sock, andar2, tamVetorReceber * sizeof(int), 0);
        }
        
        // Enviar resposta JSON
        strcpy(status_resposta.tipo, "vaga_status");
        status_resposta.livres_a1 = andar1[0] + andar1[1] + andar1[2];
        status_resposta.livres_a2 = andar2[0] + andar2[1] + andar2[2];
        status_resposta.livres_total = status_resposta.livres_a1 + status_resposta.livres_a2;
        status_resposta.flags.lotado = (andar1[20] == 1 || andar2[20] == 1);
        status_resposta.flags.bloq2 = (andar2[20] == 1);
        
        char response_json[512];
        serialize_vaga_status(&status_resposta, response_json, sizeof(response_json));
        send_json_message(client_sock, response_json);
        
        delay(1000);
    }
    close(client_sock);
    printf("Client 2 Disconnected\n");
}

void *recebeTerreo(){
    char *ip ="127.0.0.1";
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
            recv(client_sock, terreo, tamVetorReceber * sizeof(int), 0);
        }
        
        // Enviar resposta JSON
        strcpy(status_resposta.tipo, "vaga_status");
        status_resposta.livres_a1 = andar1[0] + andar1[1] + andar1[2];
        status_resposta.livres_a2 = andar2[0] + andar2[1] + andar2[2];
        status_resposta.livres_total = status_resposta.livres_a1 + status_resposta.livres_a2;
        status_resposta.flags.lotado = (andar1[20] == 1 || andar2[20] == 1);
        status_resposta.flags.bloq2 = (andar2[20] == 1);
        
        char response_json[512];
        serialize_vaga_status(&status_resposta, response_json, sizeof(response_json));
        send_json_message(client_sock, response_json);
        
        enviar[0] = terreo[12];
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
            const char* tipo_str[] = {"", "Sem correspondência", "Placa inválida", "Erro sistema"};
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
    
    pthread_t fMenu,fRecebePrimeiroAndar, fRecebeSegundoAndar, fRecebeTerreo;
    
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