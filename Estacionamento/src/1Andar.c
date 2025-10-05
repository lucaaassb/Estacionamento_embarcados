#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include "common_utils.h"
#include "json_utils.h"

//ANDAR 1
#define ENDERECO_01 RPI_V2_GPIO_P1_16                       // PINO físico 16 (GPIO23) - SAÍDA
#define ENDERECO_02 RPI_V2_GPIO_P1_38                       // PINO físico 38 (GPIO20) - SAÍDA
#define ENDERECO_03 RPI_V2_GPIO_P1_40                       // PINO físico 40 (GPIO21) - SAÍDA
#define SENSOR_DE_VAGA RPI_V2_GPIO_P1_13                    // PINO físico 13 (GPIO27) - ENTRADA
#define SENSOR_DE_PASSAGEM_1 RPI_V2_GPIO_P1_22              // PINO físico 22 (GPIO25) - ENTRADA
#define SENSOR_DE_PASSAGEM_2 RPI_V2_GPIO_P1_11              // PINO físico 11 (GPIO17) - ENTRADA
#define SINAL_DE_LOTADO_FECHADO1 RPI_V2_GPIO_P1_08          // PINO físico 8  (GPIO14) - SAÍDA

void configuraPinos1(){
    bcm2835_gpio_fsel(ENDERECO_01, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(ENDERECO_02, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(ENDERECO_03, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_DE_VAGA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SINAL_DE_LOTADO_FECHADO1, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_DE_PASSAGEM_1, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SENSOR_DE_PASSAGEM_2, BCM2835_GPIO_FSEL_INPT);
}

// Usar a estrutura melhorada do common_utils.h
typedef vaga_estacionamento_t vaga;
typedef struct vsoma{
    int somaValores;    // Soma dos valores das vagas ocupadas (ex: 1+2+3+4+5+6+7+8 = 36)
    int somaVagas;      // Soma das vagas ocupadas (ex: 1+1+1+1+1+1+1+1 = 8)
}vsoma;

vaga *vagas_andar1;
vsoma estatisticas_vagas_andar1;
int valor_sensor_vaga_andar1;        // Valor lido pelo sensor de vaga
int mudanca_vaga_andar1 = 0, total_carros_andar1 = 0;
int vagas_idoso_disponiveis_andar1 = 2, vagas_pcd_disponiveis_andar1 = 1, vagas_comum_disponiveis_andar1 = 5, andar1_fechado = 0;
int soma_valores_anterior_andar1 = 0;

#define TAMANHO_VETOR_ENVIAR 23
#define TAMANHO_VETOR_RECEBER 5

int dados_andar1[TAMANHO_VETOR_ENVIAR];
int comandos_central[TAMANHO_VETOR_RECEBER];


int separaIguala1(){
    
    dados_andar1[0]  = vagas_pcd_disponiveis_andar1;
    dados_andar1[1]  = vagas_idoso_disponiveis_andar1;
    dados_andar1[2]  = vagas_comum_disponiveis_andar1;
    dados_andar1[3]  = vagas_andar1[0].ocupada;
    dados_andar1[4]  = vagas_andar1[1].ocupada;
    dados_andar1[5]  = vagas_andar1[2].ocupada;
    dados_andar1[6]  = vagas_andar1[3].ocupada;
    dados_andar1[7]  = vagas_andar1[4].ocupada;
    dados_andar1[8]  = vagas_andar1[5].ocupada;
    dados_andar1[9]  = vagas_andar1[6].ocupada;
    dados_andar1[10] = vagas_andar1[7].ocupada;
    dados_andar1[12]= comandos_central[0];
    dados_andar1[18]= estatisticas_vagas_andar1.somaVagas;
    andar1_fechado = comandos_central[2];
    return 0;
}

void vagasOcupadas1(vaga *v){
    
    estatisticas_vagas_andar1.somaValores = 0;
    estatisticas_vagas_andar1.somaVagas = 0;
    for(int i = 0; i<8; i++){
        if(v[i].ocupada){
            estatisticas_vagas_andar1.somaVagas++;
        }
        estatisticas_vagas_andar1.somaValores += v[i].numero_vaga;
    }
    
}

void * vagasDisponiveis1(vaga *p){
    vagas_idoso_disponiveis_andar1 = 2;
    vagas_pcd_disponiveis_andar1 = 1;
    vagas_comum_disponiveis_andar1 = 5;
    
        for(int i=3; i<8; i++){
            if(p[i].ocupada)
                p[i].ocupada = 1;
            
            else if(!p[i].ocupada)
                p[i].ocupada=0;
            vagas_comum_disponiveis_andar1 -= p[i].ocupada;
        }
        for(int i=1; i<3; i++){
            if(p[i].ocupada)
                p[i].ocupada = 1;
            
            else if(!p[i].ocupada)
                p[i].ocupada=0;

            vagas_idoso_disponiveis_andar1 -= p[i].ocupada;
        }
        
        if(p[0].ocupada){
            vagas_pcd_disponiveis_andar1=0;
            p[0].ocupada = 1;
        }
        else if(!p[0].ocupada) {
            vagas_pcd_disponiveis_andar1=1;
            p[0].ocupada = 0;
        }
    return NULL;
}

int mudancaEstadoVaga1( int anteriorSomaValores1){
    return anteriorSomaValores1-estatisticas_vagas_andar1.somaValores; 
}

int timediff1(struct timeval entrada, struct timeval saida){ 
    return calcular_tempo_permanencia(entrada, saida);
}

void pagamento1(int g, vaga *a){
    
    obter_timestamp_atual(&a[g-1].horario_saida);
    a[g-1].tempo_permanencia_minutos = timediff1(a[g-1].horario_entrada, a[g-1].horario_saida);
    float valor_pago = calcular_valor_pagamento(a[g-1].tempo_permanencia_minutos);
    
    // Log do evento de saída
    evento_sistema_t evento;
    evento.timestamp = time(NULL);
    evento.tipo_evento = 2; // Saída
    evento.andar_origem = 1; // 1º Andar
    evento.andar_destino = 1;
    evento.numero_carro = a[g-1].numero_carro;
    evento.numero_vaga = g;
    strcpy(evento.placa_veiculo, a[g-1].placa_veiculo);
    evento.valor_pago = valor_pago;
    evento.confianca_leitura = a[g-1].confianca_leitura;
    
    salvar_evento_arquivo(&evento);
    {
        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg), "Carro saiu do 1º andar - Vaga: %d, Tempo: %d min, Valor: R$ %.2f", g, a[g-1].tempo_permanencia_minutos, valor_pago);
        log_info(log_msg);
    }
    
    dados_andar1[14]=1;
    dados_andar1[15]=a[g-1].numero_carro;
    dados_andar1[16]=a[g-1].tempo_permanencia_minutos;
    dados_andar1[17]=g;
    delay(1000);
    dados_andar1[14]=0;
}

void buscaCarro1(int f , vaga *a){
    f *= -1;
    a[f-1].numero_carro = dados_andar1[12];
    obter_timestamp_atual(&a[f-1].horario_entrada);
    a[f-1].numero_vaga = f;
    a[f-1].ocupada = true;
    
    // Log do evento de entrada
    evento_sistema_t evento;
    evento.timestamp = time(NULL);
    evento.tipo_evento = 1; // Entrada
    evento.andar_origem = 1; // 1º Andar
    evento.andar_destino = 1;
    evento.numero_carro = a[f-1].numero_carro;
    evento.numero_vaga = f;
    strcpy(evento.placa_veiculo, a[f-1].placa_veiculo);
    evento.valor_pago = 0.0;
    evento.confianca_leitura = a[f-1].confianca_leitura;
    
    salvar_evento_arquivo(&evento);
    {
        char log_msg[96];
        snprintf(log_msg, sizeof(log_msg), "Carro entrou no 1º andar - Vaga: %d", f);
        log_info(log_msg);
    }
    
    dados_andar1[11] = 1;
    dados_andar1[13] = f;
    delay(1000);
    dados_andar1[11] = 0;
}
// Função para detectar passagem entre andares
void *sensorPassagemA(){
    struct timeval sensor1, sensor2;
    bool sensor1_ativo = false, sensor2_ativo = false;
    
    while (1){
        // Resetar timestamps
        sensor1.tv_sec = 0;
        sensor1.tv_usec = 0;
        sensor2.tv_sec = 0;
        sensor2.tv_usec = 0;
        
        // Detectar ativação dos sensores
        if(bcm2835_gpio_lev(SENSOR_DE_PASSAGEM_1) && !sensor1_ativo){
            gettimeofday(&sensor1, 0);
            sensor1_ativo = true;
            log_debug("Sensor de passagem 1 ativado");
        }
        
        if(bcm2835_gpio_lev(SENSOR_DE_PASSAGEM_2) && !sensor2_ativo){
            gettimeofday(&sensor2, 0);
            sensor2_ativo = true;
            log_debug("Sensor de passagem 2 ativado");
        }
        
        // Verificar se ambos os sensores foram ativados
        if(sensor1_ativo && sensor2_ativo){
            long diferenca_us = sensor2.tv_usec - sensor1.tv_usec;
            long diferenca_sec = sensor2.tv_sec - sensor1.tv_sec;
            long diferenca_total_us = diferenca_sec * 1000000 + diferenca_us;
            
            if(diferenca_total_us > 0){
                // Carro subindo: 1º andar -> 2º andar
                dados_andar1[19] = 1;
                log_info("Carro subindo: 1º andar -> 2º andar");
                
                // Log do evento de passagem
                evento_sistema_t evento;
                evento.timestamp = time(NULL);
                evento.tipo_evento = 3; // Passagem
                evento.andar_origem = 1;
                evento.andar_destino = 2;
                evento.numero_carro = 0; // Não identificado
                evento.numero_vaga = 0;
                strcpy(evento.placa_veiculo, "");
                evento.valor_pago = 0.0;
                evento.confianca_leitura = 0;
                salvar_evento_arquivo(&evento);
            }
            else if(diferenca_total_us < 0){
                // Carro descendo: 2º andar -> 1º andar
                dados_andar1[19] = 2;
                log_info("Carro descendo: 2º andar -> 1º andar");
                
                // Log do evento de passagem
                evento_sistema_t evento;
                evento.timestamp = time(NULL);
                evento.tipo_evento = 3; // Passagem
                evento.andar_origem = 2;
                evento.andar_destino = 1;
                evento.numero_carro = 0; // Não identificado
                evento.numero_vaga = 0;
                strcpy(evento.placa_veiculo, "");
                evento.valor_pago = 0.0;
                evento.confianca_leitura = 0;
                salvar_evento_arquivo(&evento);
            }
            
            delay(1500);
            dados_andar1[19] = 0;
            
            // Resetar flags
            sensor1_ativo = false;
            sensor2_ativo = false;
        }
        
        // Resetar flags se sensores não estão mais ativos
        if(!bcm2835_gpio_lev(SENSOR_DE_PASSAGEM_1)){
            sensor1_ativo = false;
        }
        if(!bcm2835_gpio_lev(SENSOR_DE_PASSAGEM_2)){
            sensor2_ativo = false;
        }
        
        delay(100); // Pequeno delay para evitar polling excessivo
    }
    return 0;
}
void leituraVagasAndar1(vaga *b){
    mudanca_vaga_andar1=0;       
    estatisticas_vagas_andar1.somaVagas = 0;
    estatisticas_vagas_andar1.somaValores = 0;

    while(1){
        
        delay(50);
        vagasOcupadas1(vagas_andar1);
        vagasDisponiveis1(vagas_andar1);
        separaIguala1();

        //Primeira vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor_sensor_vaga_andar1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga_andar1 == 1)
            b[0].ocupado = 1;
            
        else if(valor_sensor_vaga_andar1 == 0) 
            b[0].ocupado = 0;
        
        //Segunda vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor_sensor_vaga_andar1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga_andar1 == 1)
            b[1].ocupado = 2;
        else if(valor_sensor_vaga_andar1 == 0) 
            b[1].ocupado = 0;

        //Terceira vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor_sensor_vaga_andar1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga_andar1 == 1)
            b[2].ocupado = 3;
        else if(valor_sensor_vaga_andar1 == 0) 
            b[2].ocupado = 0;        

        //Quarta vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor_sensor_vaga_andar1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga_andar1 == 1)
            b[3].ocupado = 4;
        else if(valor_sensor_vaga_andar1 == 0) 
            b[3].ocupado = 0;        

        //Quinta vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor_sensor_vaga_andar1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga_andar1 == 1)
            b[4].ocupado = 5;
        else if(valor_sensor_vaga_andar1 == 0) 
            b[4].ocupado = 0;        

        //Sexta vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor_sensor_vaga_andar1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga_andar1 == 1)
            b[5].ocupado = 6;
        else if(valor_sensor_vaga_andar1 == 0) 
            b[5].ocupado = 0;        

        //Sétima vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor_sensor_vaga_andar1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga_andar1 == 1)
            b[6].ocupado = 7;
        else if(valor_sensor_vaga_andar1 == 0) 
            b[6].ocupado = 0;
 

        //Oitava vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor_sensor_vaga_andar1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga_andar1 == 1)
            b[7].ocupado = 8;
        else if(valor_sensor_vaga_andar1 == 0) 
            b[7].ocupado = 0;

//-------------------------------------------------------------//
        mudanca_vaga_andar1 = mudancaEstadoVaga1(soma_valores_anterior_andar1);
        if(mudanca_vaga_andar1>0 && mudanca_vaga_andar1<9){
            pagamento1(mudanca_vaga_andar1, b);
            
        }else if(mudanca_vaga_andar1<0 && mudanca_vaga_andar1>-9){
            buscaCarro1(mudanca_vaga_andar1, b);
        } 
        soma_valores_anterior_andar1 = estatisticas_vagas_andar1.somaValores;
        
        if((estatisticas_vagas_andar1.somaVagas < 8 && andar1_fechado==0)){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO1, LOW);
            dados_andar1[20] = 0;
        }
        else if(estatisticas_vagas_andar1.somaVagas==8){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO1, HIGH);
            dados_andar1[20] = 1;
        } 
        
        else if(andar1_fechado==1){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO1, HIGH);
            dados_andar1[20] = 1;
        } 
        else if(andar1_fechado == 0){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO1, LOW);
            dados_andar1[20] = 0;
        } 
    }
}

void *chamaLeitura1(){
    leituraVagasAndar1(vagas_andar1);
    return NULL;
}

void *enviaParametros1(){
    char *ip ="164.41.98.2";  // IP do servidor central
    int port = 10681;
    
    int sock;
    struct sockaddr_in addr;
    // Tamanho do endereço não utilizado diretamente
    // socklen_t addr_size;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        perror("[-]Socket error");
        exit(1);
    }
    printf("[+]Server socket created\n");

    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    printf("Connected to Server\n");
    
    char json_buffer[1024];
    vaga_status_t status_recebido;
    
    while(1){
        // Enviar dados via JSON (passagem entre andares)
        if (dados_andar1[19] == 1) { // Carro em trânsito
            passagem_t passagem;
            strcpy(passagem.tipo, "passagem");
            passagem.andar_origem = 1;
            passagem.andar_destino = 2;
            get_current_timestamp(passagem.ts, sizeof(passagem.ts));
            
            char passagem_json[512];
            serialize_passagem(&passagem, passagem_json, sizeof(passagem_json));
            send_json_message(sock, passagem_json);
        }
        
        // Manter compatibilidade com arrays antigos
        send (sock, dados_andar1, TAMANHO_VETOR_ENVIAR *sizeof(int) , 0);
        
        // Receber resposta JSON do servidor central
        if (receive_json_message(sock, json_buffer, sizeof(json_buffer)) == 0) {
            printf("Recebido do central: %s\n", json_buffer);
            if (deserialize_vaga_status(json_buffer, &status_recebido) == 0) {
                printf("Status das vagas: A1=%d, A2=%d, Total=%d\n", 
                       status_recebido.livres_a1, status_recebido.livres_a2, status_recebido.livres_total);
            }
        }
        
        recv(sock, comandos_central, TAMANHO_VETOR_RECEBER * sizeof(int), 0);
        delay(1000);
    }
    close(sock);
    printf("Disconnected from server\n");
}

int mainU(){
    // Inicializar sistema de logs
    init_log_system();
    log_info("Iniciando servidor do 1º andar");
    
    // Inicializar BCM2835
    if (!bcm2835_init()) {
        log_erro("Falha ao inicializar BCM2835");
        return 1;
    }
    
    // Configurar pinos GPIO
    configuraPinos1();
    log_info("Pinos GPIO do 1º andar configurados");
    
    // Inicializar variáveis
    vagas_andar1 = calloc(8, sizeof(vaga));
    if (!vagas_andar1) {
        log_erro("Falha ao alocar memória para vagas do 1º andar");
        bcm2835_close();
        return 1;
    }
    
    pthread_t fLeituraVagas1, fEnviaParametros1, fSensorPassagemA;
    
    log_info("Criando threads do servidor 1º andar");
    pthread_create(&fLeituraVagas1, NULL, chamaLeitura1, NULL);
    pthread_create(&fEnviaParametros1, NULL, enviaParametros1, NULL);
    pthread_create(&fSensorPassagemA, NULL, sensorPassagemA, NULL); // Ativado!
    
    log_info("Servidor 1º andar em execução");
    pthread_join(fLeituraVagas1, NULL);
    pthread_join(fEnviaParametros1, NULL);
    pthread_join(fSensorPassagemA, NULL);
    
    // Limpeza
    free(vagas_andar1);
    close_log_system();
    bcm2835_close();
    
    log_info("Servidor 1º andar finalizado");
    return 0;
}