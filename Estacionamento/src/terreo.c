#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include "modbus_utils.h"
#include "common_utils.h"
#include "json_utils.h"
#include "thingsboard.h"


// ANDAR TÉRREO - Conforme Tabela 1 do README
#define ENDERECO_01 RPI_V2_GPIO_P1_11                       // PINO físico 11 (GPIO17) - SAÍDA
#define ENDERECO_02 RPI_V2_GPIO_P1_12                       // PINO físico 12 (GPIO18) - SAÍDA
#define SENSOR_DE_VAGA RPI_V2_GPIO_P1_24                    // PINO físico 24 (GPIO8)  - ENTRADA
#define SENSOR_ABERTURA_CANCELA_ENTRADA RPI_V2_GPIO_P1_07   // PINO físico 7  (GPIO4)  - ENTRADA - README: GPIO7
#define SENSOR_FECHAMENTO_CANCELA_ENTRADA RPI_V2_GPIO_P1_28 // PINO físico 28 (GPIO1)  - ENTRADA
#define MOTOR_CANCELA_ENTRADA RPI_V2_GPIO_P1_16             // PINO físico 16 (GPIO23) - SAÍDA
#define SENSOR_ABERTURA_CANCELA_SAIDA RPI_V2_GPIO_P1_32     // PINO físico 32 (GPIO12) - ENTRADA
#define SENSOR_FECHAMENTO_CANCELA_SAIDA RPI_V2_GPIO_P1_22   // PINO físico 22 (GPIO25) - ENTRADA
#define MOTOR_CANCELA_SAIDA RPI_V2_GPIO_P1_18               // PINO físico 18 (GPIO24) - SAÍDA
#define SINAL_DE_LOTADO_FECHADO RPI_V2_GPIO_P1_26           // PINO físico 26 (GPIO7)  - SAÍDA

void configuraPinos(){
    bcm2835_gpio_fsel(ENDERECO_01, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(ENDERECO_02, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_DE_VAGA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SENSOR_ABERTURA_CANCELA_ENTRADA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SENSOR_FECHAMENTO_CANCELA_ENTRADA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(MOTOR_CANCELA_ENTRADA, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_ABERTURA_CANCELA_SAIDA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SENSOR_FECHAMENTO_CANCELA_SAIDA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(MOTOR_CANCELA_SAIDA, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SINAL_DE_LOTADO_FECHADO, BCM2835_GPIO_FSEL_OUTP);
}
// Usar a estrutura melhorada do common_utils.h
typedef vaga_estacionamento_t vaga;

typedef struct vsoma{
    int somaValores;    // Soma dos valores das vagas ocupadas (ex: 1+2+3+4+5+6+7+8 = 36)
    int somaVagas;      // Soma das vagas ocupadas (ex: 1+1+1+1+1+1+1+1 = 8)
}vsoma;

// Variáveis do sistema
int contador_carros = 0;
int valor_sensor_vaga;              // Valor lido pelo sensor de vaga
vaga *vagas_terreo;
vsoma estatisticas_vagas;
int mudanca_vaga = 0, flag_entrada = 0, total_carros_terreo = 0;
int vagas_idoso_disponiveis = 2, vagas_pcd_disponiveis = 1, vagas_comum_disponiveis = 5;
int soma_valores_anterior = 0;

// Variáveis para ThingsBoard
static int carros_entrada_total = 0, carros_saida_total = 0;
static float valor_arrecadado_total = 0.0;

// Variáveis MODBUS
modbus_t* ctx_modbus = NULL;
lpr_data_t dados_camera_entrada;
lpr_data_t dados_camera_saida;
placar_data_t dados_placar;
char matricula_usuario[10] = "202017700"; // Matrícula do usuário

#define TAMANHO_VETOR_ENVIAR 22
#define TAMANHO_VETOR_RECEBER 5

static int dados_terreo[TAMANHO_VETOR_ENVIAR];
static int comandos_central[TAMANHO_VETOR_RECEBER];
int fechado = 0;

int separaIguala(){
    
    dados_terreo[0] = vagas_pcd_disponiveis;
    dados_terreo[1] = vagas_idoso_disponiveis;
    dados_terreo[2] = vagas_comum_disponiveis;
    dados_terreo[3] = vagas_terreo[0].ocupada;
    dados_terreo[4] = vagas_terreo[1].ocupada;
    dados_terreo[5] = vagas_terreo[2].ocupada;
    dados_terreo[6] = vagas_terreo[3].ocupada;
    dados_terreo[7] = vagas_terreo[4].ocupada;
    dados_terreo[8] = vagas_terreo[5].ocupada;
    dados_terreo[9] = vagas_terreo[6].ocupada;
    dados_terreo[10] = vagas_terreo[7].ocupada;
    dados_terreo[12] = total_carros_terreo;
    dados_terreo[18] = estatisticas_vagas.somaVagas;
    fechado = comandos_central[1];
    dados_terreo[19] = comandos_central[4];    
    return 0;
}

// Função para atualizar placar MODBUS
void atualizar_placar_modbus() {
    if (!ctx_modbus) return;
    
    // Preparar dados do placar
    dados_placar.vagas_terreo_pcd = vagas_pcd_disponiveis;
    dados_placar.vagas_terreo_idoso = vagas_idoso_disponiveis;
    dados_placar.vagas_terreo_comum = vagas_comum_disponiveis;
    
    // Dados dos outros andares (recebidos do servidor central)
    dados_placar.vagas_andar1_pcd = dados_terreo[0]; // Será atualizado pelo central
    dados_placar.vagas_andar1_idoso = dados_terreo[1];
    dados_placar.vagas_andar1_comum = dados_terreo[2];
    dados_placar.vagas_andar2_pcd = dados_terreo[0]; // Será atualizado pelo central
    dados_placar.vagas_andar2_idoso = dados_terreo[1];
    dados_placar.vagas_andar2_comum = dados_terreo[2];
    
    // Totais
    dados_placar.vagas_total_pcd = dados_placar.vagas_terreo_pcd + dados_placar.vagas_andar1_pcd + dados_placar.vagas_andar2_pcd;
    dados_placar.vagas_total_idoso = dados_placar.vagas_terreo_idoso + dados_placar.vagas_andar1_idoso + dados_placar.vagas_andar2_idoso;
    dados_placar.vagas_total_comum = dados_placar.vagas_terreo_comum + dados_placar.vagas_andar1_comum + dados_placar.vagas_andar2_comum;
    
    // Flags
    dados_placar.flags = 0;
    if (fechado) dados_placar.flags |= 0x01; // bit0 = lotado geral
    if (comandos_central[2]) dados_placar.flags |= 0x02; // bit1 = lotado 1º andar
    if (comandos_central[3]) dados_placar.flags |= 0x04; // bit2 = lotado 2º andar
    
    // Atualizar placar via MODBUS com retry
    uint16_t registers[13];
    registers[0] = dados_placar.vagas_terreo_pcd;
    registers[1] = dados_placar.vagas_terreo_idoso;
    registers[2] = dados_placar.vagas_terreo_comum;
    registers[3] = dados_placar.vagas_andar1_pcd;
    registers[4] = dados_placar.vagas_andar1_idoso;
    registers[5] = dados_placar.vagas_andar1_comum;
    registers[6] = dados_placar.vagas_andar2_pcd;
    registers[7] = dados_placar.vagas_andar2_idoso;
    registers[8] = dados_placar.vagas_andar2_comum;
    registers[9] = dados_placar.vagas_total_pcd;
    registers[10] = dados_placar.vagas_total_idoso;
    registers[11] = dados_placar.vagas_total_comum;
    registers[12] = dados_placar.flags;
    
    if (modbus_set_slave(ctx_modbus, PLACAR_ADDR) == 0) {
        if (modbus_write_registers_with_retry(ctx_modbus, 0, 13, registers, 3) != -1) {
            // Enviar matrícula após atualização com retry
            if (send_matricula_modbus(ctx_modbus, PLACAR_ADDR, matricula_usuario) == 0) {
                log_info("Placar MODBUS atualizado com sucesso");
            } else {
                log_erro("Falha ao enviar matrícula para placar");
            }
        } else {
            log_erro("Falha ao atualizar placar MODBUS após tentativas");
        }
    } else {
        log_erro("Falha ao definir endereço do placar MODBUS");
    }
}

//Função que lê o sensor da cancela de entrada quando um carro está entrando no estacionamento
void * sensorEntrada(){
    while(1){
        
        if(fechado==0){

        //Lê o sensor de abertura da cancela de entrada e aciona o motor da cancela para abrir
        if(HIGH == bcm2835_gpio_lev(SENSOR_ABERTURA_CANCELA_ENTRADA)){
            bcm2835_gpio_write(MOTOR_CANCELA_ENTRADA, HIGH);
            dados_terreo[19]=1;
            
            // Disparar captura da câmera de entrada com retry
            if (ctx_modbus) {
                if (capture_license_plate(ctx_modbus, CAMERA_ENTRADA_ADDR, &dados_camera_entrada, 2000) == 0) {
                    log_info("Placa capturada na entrada");
                    
                    // Implementar política de tratamento de confiança conforme README
                    if (validar_placa(dados_camera_entrada.placa) && dados_camera_entrada.confianca >= 60) {
                        if (dados_camera_entrada.confianca >= 70) {
                            log_info("Placa válida capturada na entrada com alta confiança");
                        } else {
                            // Confiança entre 60-69%: permitir entrada com ticket temporário
                            char log_msg[128];
                            snprintf(log_msg, sizeof(log_msg), 
                                    "Placa com confiança média (%d%%) - gerando ticket temporário", 
                                    dados_camera_entrada.confianca);
                            log_info(log_msg);
                            
                            int ticket_id = gerar_ticket_temporario(dados_camera_entrada.placa, 
                                                                  dados_camera_entrada.confianca, 
                                                                  0, 0); // Vaga será definida quando estacionar
                            if (ticket_id > 0) {
                                snprintf(log_msg, sizeof(log_msg), 
                                        "Ticket temporário gerado para entrada: %d", ticket_id);
                                log_info(log_msg);
                            }
                        }
                    } else {
                        // Confiança < 60%: gerar ticket com ID anônimo
                        log_erro("Placa inválida ou confiança muito baixa (<60%) na entrada");
                        char placa_anonima[16];
                        snprintf(placa_anonima, sizeof(placa_anonima), "ANOM%04d", rand() % 10000);
                        
                        int ticket_id = gerar_ticket_temporario(placa_anonima, 
                                                              dados_camera_entrada.confianca, 
                                                              0, 0);
                        if (ticket_id > 0) {
                            char log_msg[128];
                            snprintf(log_msg, sizeof(log_msg), 
                                    "Ticket anônimo gerado para entrada: %d (%s)", 
                                    ticket_id, placa_anonima);
                            log_info(log_msg);
                        }
                    }
                } else {
                    log_erro("Falha na captura da placa na entrada");
                    // Gerar ticket temporário para falha de captura
                    char placa_falha[16];
                    snprintf(placa_falha, sizeof(placa_falha), "FALHA%04d", rand() % 10000);
                    
                    int ticket_id = gerar_ticket_temporario(placa_falha, 0, 0, 0);
                    if (ticket_id > 0) {
                        char log_msg[128];
                        snprintf(log_msg, sizeof(log_msg), 
                                "Ticket de falha gerado para entrada: %d (%s)", 
                                ticket_id, placa_falha);
                        log_info(log_msg);
                    }
                }
            }
        }
        //Lê o sensor de saída da cancela e aciona o motor da cancela para fechar
        if(HIGH == bcm2835_gpio_lev(SENSOR_FECHAMENTO_CANCELA_ENTRADA)){
            bcm2835_gpio_write(MOTOR_CANCELA_ENTRADA, LOW);
            if(flag_entrada==0){
                ++total_carros_terreo;
                ++carros_entrada_total;
                flag_entrada=1;
                dados_terreo[19]=1;
                
                // Enviar evento de entrada para ThingsBoard
                send_vehicle_event(TB_ACCESS_TOKEN_TERREO, "entrada", 
                                 dados_camera_entrada.placa, 0, 0.0);
                log_info("Evento de entrada enviado para dashboard");
            }
        }else flag_entrada=0;
        
        }
            
    }
    return NULL;
}

//Função que lê o sensor da cancela de saída quando um carro está saindo do estacionamento
void * sensorSaida(){
    while(1){
         //Lê o sensor de abertura da cancela de saida e aciona o motor da cancela para abrir
        if(HIGH == bcm2835_gpio_lev(SENSOR_ABERTURA_CANCELA_SAIDA)){
            bcm2835_gpio_write(MOTOR_CANCELA_SAIDA, HIGH);
            
            // Disparar captura da câmera de saída com retry
            if (ctx_modbus) {
                if (capture_license_plate(ctx_modbus, CAMERA_SAIDA_ADDR, &dados_camera_saida, 2000) == 0) {
                    log_info("Placa capturada na saída");
                    
                    // Aplicar mesma política de confiança da entrada
                    if (validar_placa(dados_camera_saida.placa) && dados_camera_saida.confianca >= 60) {
                        if (dados_camera_saida.confianca >= 70) {
                            log_info("Placa válida capturada na saída com alta confiança");
                        } else {
                            char log_msg[128];
                            snprintf(log_msg, sizeof(log_msg), 
                                    "Placa na saída com confiança média (%d%%)", 
                                    dados_camera_saida.confianca);
                            log_info(log_msg);
                        }
                        
                        // Verificar correspondência com entrada (incluir tickets temporários)
                        if (!buscar_correspondencia_entrada(dados_camera_saida.placa)) {
                            // Verificar se existe ticket temporário correspondente
                            ticket_temporario_t* ticket = buscar_ticket_por_placa(dados_camera_saida.placa);
                            if (!ticket) {
                                sinalizar_alerta_auditoria(dados_camera_saida.placa, 
                                                        "Carro sem correspondência de entrada ou ticket", 1);
                            } else {
                                char log_msg[128];
                                snprintf(log_msg, sizeof(log_msg), 
                                        "Saída associada ao ticket temporário: %d", ticket->ticket_id);
                                log_info(log_msg);
                                desativar_ticket(ticket->ticket_id);
                            }
                        }
                    } else {
                        // Confiança < 60%: tentar buscar por tickets
                        log_erro("Placa inválida ou confiança muito baixa (<60%) na saída");
                        
                        // Buscar tickets ativos para reconciliação manual
                        ticket_temporario_t tickets[10];
                        int num_tickets = listar_tickets_ativos(tickets, 10);
                        if (num_tickets > 0) {
                            log_info("Tickets ativos encontrados para possível reconciliação");
                        } else {
                            sinalizar_alerta_auditoria(dados_camera_saida.placa, 
                                                    "Placa inválida na saída sem tickets ativos", 2);
                        }
                    }
                } else {
                    log_erro("Falha na captura da placa na saída");
                    sinalizar_alerta_auditoria("DESCONHECIDA", 
                                            "Falha na captura da placa na saída", 3);
                }
            }
        }
        //Lê o sensor de saída da cancela de saída e aciona o motor da cancela para fechar
        if(HIGH == bcm2835_gpio_lev(SENSOR_FECHAMENTO_CANCELA_SAIDA)){
            bcm2835_gpio_write(MOTOR_CANCELA_SAIDA, LOW);
            dados_terreo[19]=0;
            ++carros_saida_total;
            
            // Enviar evento de saída para ThingsBoard
            send_vehicle_event(TB_ACCESS_TOKEN_TERREO, "saida", 
                             dados_camera_saida.placa, 0, 0.0);
            log_info("Evento de saída enviado para dashboard");
        }
    }
    return NULL;
}

//Função que verifica quais vagas estão ocupadas (TÉRREO: 4 vagas)
void vagasOcupadas(vaga *v){
    
    estatisticas_vagas.somaValores = 0;
    estatisticas_vagas.somaVagas = 0;
    for(int i = 0; i<4; i++){  // Ajustado para 4 vagas
        if(v[i].ocupada){
            estatisticas_vagas.somaVagas++;
        }
        estatisticas_vagas.somaValores += v[i].numero_vaga;
    }
}

//Função que verifica e imprime as vagas disponíveis por tipo (TÉRREO: 4 vagas)
void * vagasDisponiveis(vaga *v){
    vagas_idoso_disponiveis = 1;  // 1 vaga para idoso
    vagas_pcd_disponiveis = 1;    // 1 vaga para PCD
    vagas_comum_disponiveis = 2;  // 2 vagas comuns (total: 4 vagas)
    
        // Vagas comuns (vagas 3 e 4)
        for(int i=2; i<4; i++){
            if(v[i].ocupada)
                v[i].ocupada = 1;
            else if(!v[i].ocupada)
                v[i].ocupada=0;
            vagas_comum_disponiveis -= v[i].ocupada;
        }
        
        // Vaga idoso (vaga 2)
        if(v[1].ocupada)
            v[1].ocupada = 1;
        else if(!v[1].ocupada)
            v[1].ocupada=0;
        vagas_idoso_disponiveis -= v[1].ocupada;
        
        // Vaga PCD (vaga 1)
        if(v[0].ocupada){
            vagas_pcd_disponiveis=0;
            v[0].ocupada = 1;
        }
        else if(!v[0].ocupada) {
            vagas_pcd_disponiveis=1;
            v[0].ocupada=0;
        }
    return NULL;
}

//Função que verifica a mudança de estado das vagas
int mudancaEstadoVaga(vsoma *s, int anteriorSomaValores){
    return anteriorSomaValores-s->somaValores; 
}

//Função que calcula o tempo de permanência do carro na vaga
int timediff(struct timeval entrada, struct timeval saida){
    return calcular_tempo_permanencia(entrada, saida);
}

//Função que calcula o valor a ser pago pelo carro que estava na vaga
void pagamento(int g, vaga *v){
    obter_timestamp_atual(&v[g-1].horario_saida);
    v[g-1].tempo_permanencia_minutos = timediff(v[g-1].horario_entrada, v[g-1].horario_saida);
    float valor_pago = calcular_valor_pagamento(v[g-1].tempo_permanencia_minutos);
    
    // Log do evento de saída
    evento_sistema_t evento;
    evento.timestamp = time(NULL);
    evento.tipo_evento = 2; // Saída
    evento.andar_origem = 0; // Térreo
    evento.andar_destino = 0;
    evento.numero_carro = v[g-1].numero_carro;
    evento.numero_vaga = g;
    strcpy(evento.placa_veiculo, v[g-1].placa_veiculo);
    evento.valor_pago = valor_pago;
    evento.confianca_leitura = v[g-1].confianca_leitura;
    
    salvar_evento_arquivo(&evento);
    {
        char log_msg[112];
        snprintf(log_msg, sizeof(log_msg), "Carro saiu - Vaga: %d, Tempo: %d min, Valor: R$ %.2f", g, v[g-1].tempo_permanencia_minutos, valor_pago);
        log_info(log_msg);
    }
    
    // Acumular valor arrecadado e enviar para ThingsBoard
    valor_arrecadado_total += valor_pago;
    send_vehicle_event(TB_ACCESS_TOKEN_TERREO, "cobranca", v[g-1].placa_veiculo, g, valor_pago);
    
    dados_terreo[14]=1;
    dados_terreo[15]=v[g-1].numero_carro;
    dados_terreo[16]=v[g-1].tempo_permanencia_minutos;
    dados_terreo[17]=g;
    delay(1000);
    dados_terreo[14]=0;
}

//Função que verifica em qual vaga o carro estacionou
void buscaCarro(int f , vaga *v){
    f *= -1;
    v[f-1].numero_carro = total_carros_terreo;
    obter_timestamp_atual(&v[f-1].horario_entrada);
    v[f-1].numero_vaga = f;
    v[f-1].ocupada = true;
    
    // Copiar dados da placa se disponível
    if (strlen(dados_camera_entrada.placa) > 0) {
        strcpy(v[f-1].placa_veiculo, dados_camera_entrada.placa);
        v[f-1].confianca_leitura = dados_camera_entrada.confianca;
    }
    
    // Log do evento de entrada
    evento_sistema_t evento;
    evento.timestamp = time(NULL);
    evento.tipo_evento = 1; // Entrada
    evento.andar_origem = 0; // Térreo
    evento.andar_destino = 0;
    evento.numero_carro = v[f-1].numero_carro;
    evento.numero_vaga = f;
    strcpy(evento.placa_veiculo, v[f-1].placa_veiculo);
    evento.valor_pago = 0.0;
    evento.confianca_leitura = v[f-1].confianca_leitura;
    
    salvar_evento_arquivo(&evento);
    {
        char log_msg[96];
        snprintf(log_msg, sizeof(log_msg), "Carro entrou - Vaga: %d, Placa: %s", f, v[f-1].placa_veiculo);
        log_info(log_msg);
    }
    
    dados_terreo[11] = 1;
    dados_terreo[13] = f;

    delay(1000);
    dados_terreo[11] = 0;
}

//Função que lê o estado das vagas do terreo
void leituraVagasTerreo(vaga *v){
    mudanca_vaga=0;
    soma_valores_anterior = 0;        
    estatisticas_vagas.somaVagas = 0;
    estatisticas_vagas.somaValores = 0;

    while(1){
        delay(50);
        delay(50);
        vagasOcupadas(v);
        vagasDisponiveis(v);
        separaIguala();

        //Primeira vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        delay(50);
        valor_sensor_vaga = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga == 1)
            v[0].ocupado = 1;

        else if(valor_sensor_vaga == 0) 
            v[0].ocupado = 0;
        
        //Segunda vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        delay(50);
        valor_sensor_vaga = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga == 1)
            v[1].ocupado = 2;
        else if(valor_sensor_vaga == 0) 
            v[1].ocupado = 0;

        //Terceira vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        delay(50);
        valor_sensor_vaga = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga == 1)
            v[2].ocupado = 3;
        else if(valor_sensor_vaga == 0) 
            v[2].ocupado = 0;        

        //Quarta vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        delay(50);
        valor_sensor_vaga = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor_sensor_vaga == 1)
            v[3].ocupado = 4;
        else if(valor_sensor_vaga == 0) 
            v[3].ocupado = 0;        

        // Térreo possui apenas 4 vagas (1 PCD, 1 Idoso, 2 Comuns)
            
        mudanca_vaga = mudancaEstadoVaga(&estatisticas_vagas, soma_valores_anterior);
        dados_terreo[16]=0;
        if(mudanca_vaga>0 && mudanca_vaga<5){  // Ajustado para 4 vagas
            dados_terreo[19] = 1;
            pagamento(mudanca_vaga, v);
        }else if(mudanca_vaga<0 && mudanca_vaga>-5){  // Ajustado para 4 vagas
            dados_terreo[19] = 0;
            buscaCarro(mudanca_vaga, v);
        } 
        soma_valores_anterior = estatisticas_vagas.somaValores;
    
        if(fechado == 1) {
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO, HIGH);
        }
        else if(fechado == 0) {
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO, LOW);
        }
        
        // Atualizar placar MODBUS e ThingsBoard periodicamente
        static int contador_placar = 0;
        if (++contador_placar >= 10) { // A cada 10 ciclos (500ms)
            atualizar_placar_modbus();
            
            // Enviar dados para ThingsBoard
            int vagas_array[4] = {v[0].ocupada, v[1].ocupada, v[2].ocupada, v[3].ocupada};
            send_terreo_data(vagas_array, carros_entrada_total, carros_saida_total, valor_arrecadado_total);
            
            contador_placar = 0;
        }
    }
}

void *chamaLeitura(){
    leituraVagasTerreo(vagas_terreo);
    return NULL;
}

// Função auxiliar para reconexão TCP/IP
bool tentar_reconexao_tcp(int *sock, struct sockaddr_in *addr, int *tentativas) {
    if (*sock != -1) {
        close(*sock);
        *sock = -1;
    }
    
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if(*sock < 0){
        log_erro("Erro ao criar socket");
        return false;
    }
    
    // Configurar timeout do socket
    struct timeval timeout;
    timeout.tv_sec = 5;  // 5 segundos
    timeout.tv_usec = 0;
    setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    setsockopt(*sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    
    if(connect(*sock, (struct sockaddr*)addr, sizeof(*addr)) == 0){
        log_info("Reconectado ao servidor central");
        *tentativas = 0;
        return true;
    } else {
        char erro_msg[128];
        snprintf(erro_msg, sizeof(erro_msg), "Falha na reconexão (tentativa %d)", (*tentativas)++);
        log_erro(erro_msg);
        close(*sock);
        *sock = -1;
        return false;
    }
}

void *enviaParametros(){
    char *ip ="164.41.98.2";  // IP do servidor central
    int port = 10683;
    
    int sock = -1;
    struct sockaddr_in addr;
    bool conectado = false;
    int tentativas_reconexao = 0;
    const int max_tentativas = 5;
    
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    
    // Tentar conexão inicial
    conectado = tentar_reconexao_tcp(&sock, &addr, &tentativas_reconexao);
    
    char json_buffer[1024];
    vaga_status_t status_recebido;
    
    while(1){
        // Verificar se precisa reconectar
        if (!conectado || sock == -1) {
            if (tentativas_reconexao < max_tentativas) {
                log_info("Tentando reconectar ao servidor central...");
                conectado = tentar_reconexao_tcp(&sock, &addr, &tentativas_reconexao);
                if (!conectado) {
                    // Backoff exponencial para reconexão
                    int delay_reconexao = 1000 * (1 << tentativas_reconexao); // 1s, 2s, 4s, 8s, 16s
                    if (delay_reconexao > 30000) delay_reconexao = 30000; // Máximo 30s
                    usleep(delay_reconexao * 1000);
                    continue;
                }
            } else {
                log_erro("Máximo de tentativas de reconexão excedido - modo degradado");
                sleep(60); // Aguardar 1 minuto antes de tentar novamente
                tentativas_reconexao = 0;
                continue;
            }
        }
        // Enviar dados via JSON (entrada, saída, passagem, etc.)
        if (dados_terreo[11] == 1) { // Carro entrando
            entrada_ok_t entrada;
            strcpy(entrada.tipo, "entrada_ok");
            strcpy(entrada.placa, "ABC1234"); // Placa da câmera LPR
            entrada.conf = 85;
            get_current_timestamp(entrada.ts, sizeof(entrada.ts));
            entrada.andar = 0; // Térreo
            
            char entrada_json[512];
            serialize_entrada_ok(&entrada, entrada_json, sizeof(entrada_json));
            send_json_message(sock, entrada_json);
        }
        
        if (dados_terreo[14] == 1) { // Carro saindo
            saida_ok_t saida;
            strcpy(saida.tipo, "saida_ok");
            strcpy(saida.placa, "ABC1234"); // Placa da câmera LPR
            saida.conf = 82;
            get_current_timestamp(saida.ts, sizeof(saida.ts));
            saida.andar = 0; // Térreo
            
            char saida_json[512];
            serialize_saida_ok(&saida, saida_json, sizeof(saida_json));
            send_json_message(sock, saida_json);
        }
        
        // Tentar enviar dados com tratamento de erro
        if (send(sock, dados_terreo, TAMANHO_VETOR_ENVIAR * sizeof(int), 0) == -1) {
            log_erro("Erro ao enviar dados para servidor central");
            conectado = false;
            continue;
        }
        
        // Receber resposta JSON do servidor central
        if (receive_json_message(sock, json_buffer, sizeof(json_buffer)) == 0) {
            printf("Recebido do central: %s\n", json_buffer);
            if (deserialize_vaga_status(json_buffer, &status_recebido) == 0) {
                printf("Status das vagas: A1=%d, A2=%d, Total=%d\n", 
                       status_recebido.livres_a1, status_recebido.livres_a2, status_recebido.livres_total);
            }
        }
        
        // Receber comandos com tratamento de erro
        if (recv(sock, comandos_central, TAMANHO_VETOR_RECEBER * sizeof(int), 0) == -1) {
            log_erro("Erro ao receber comandos do servidor central");
            conectado = false;
            continue;
        }
        
        delay(1000);
    }
    close(sock);
    printf("Disconnected from server\n");
    return NULL;
}

int mainT(){
    // Inicializar sistema de logs
    init_log_system();
    log_info("Iniciando servidor do andar térreo");
    
    // Inicializar BCM2835
    if (!bcm2835_init()) {
        log_erro("Falha ao inicializar BCM2835");
        return 1;
    }

    // Configurar pinos GPIO
    configuraPinos();
    log_info("Pinos GPIO configurados");
    
    // Inicializar MODBUS
    ctx_modbus = init_modbus_connection("/dev/ttyUSB0", 115200);
    if (!ctx_modbus) {
        log_erro("Falha ao inicializar MODBUS - continuando sem câmeras");
    } else {
        log_info("MODBUS inicializado com sucesso");
    }
    
    // Inicializar ThingsBoard
    if (init_thingsboard_client() == 0) {
        log_info("Cliente ThingsBoard inicializado com sucesso");
    } else {
        log_erro("Falha ao inicializar cliente ThingsBoard - dashboard não funcionará");
    }

    // Inicializar variáveis
    contador_carros = 0;
    vagas_terreo = calloc(4, sizeof(vaga));  // Ajustado para 4 vagas
    if (!vagas_terreo) {
        log_erro("Falha ao alocar memória para vagas");
        bcm2835_close();
        return 1;
    }

    // Inicializar dados do placar
    memset(&dados_placar, 0, sizeof(placar_data_t));

    pthread_t fEntrada, fSaida, fLeituraVagas, fEnviaParametros;

    log_info("Criando threads do servidor térreo");
    pthread_create(&fLeituraVagas, NULL, chamaLeitura, NULL);
    pthread_create(&fEnviaParametros, NULL, enviaParametros, NULL);
    pthread_create(&fEntrada,NULL,sensorEntrada,NULL);
    pthread_create(&fSaida,NULL,sensorSaida,NULL);

    log_info("Servidor térreo em execução");
    pthread_join(fEntrada,NULL);
    pthread_join(fSaida,NULL);
    pthread_join(fLeituraVagas, NULL);
    pthread_join(fEnviaParametros, NULL);

    // Limpeza
    if (ctx_modbus) {
        close_modbus_connection(ctx_modbus);
    }
    cleanup_thingsboard_client();
    free(vagas_terreo);
    close_log_system();
    bcm2835_close();
    
    log_info("Servidor térreo finalizado");
    return 0;
}