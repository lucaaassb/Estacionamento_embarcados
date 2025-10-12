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
#include "../inc/lpr_terreo.h"
#include "../inc/modbus.h"


//ANDAR TÉRREO
#define ENDERECO_01 17                                          // GPIO 17 - SAÍDA
#define ENDERECO_02 18                                          // GPIO 18 - SAÍDA
#define SENSOR_DE_VAGA 8                                        // GPIO 08 - ENTRADA
#define SINAL_DE_LOTADO_FECHADO RPI_V2_GPIO_P1_13               // PINO 27 - SAÍDA // Comentar essa linha se não for usado
#define SENSOR_ABERTURA_CANCELA_ENTRADA 7                       // GPIO 07 - ENTRADA
#define SENSOR_FECHAMENTO_CANCELA_ENTRADA 1                     // GPIO 01 - ENTRADA
#define MOTOR_CANCELA_ENTRADA 23                                // GPIO 23 - SAÍDA
#define SENSOR_ABERTURA_CANCELA_SAIDA 12                        // GPIO 12 - ENTRADA
#define SENSOR_FECHAMENTO_CANCELA_SAIDA 25                      // GPIO 25 - ENTRADA
#define MOTOR_CANCELA_SAIDA 24                                  // GPIO 24 - SAÍDA

void configuraPinos(){
    bcm2835_gpio_fsel(ENDERECO_01, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(ENDERECO_02, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_DE_VAGA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SINAL_DE_LOTADO_FECHADO, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_ABERTURA_CANCELA_ENTRADA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SENSOR_FECHAMENTO_CANCELA_ENTRADA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(MOTOR_CANCELA_ENTRADA, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_ABERTURA_CANCELA_SAIDA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SENSOR_FECHAMENTO_CANCELA_SAIDA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(MOTOR_CANCELA_SAIDA, BCM2835_GPIO_FSEL_OUTP);
    
    // Configura pull-down nos sensores para evitar leituras falsas
    bcm2835_gpio_set_pud(SENSOR_DE_VAGA, BCM2835_GPIO_PUD_DOWN);
    bcm2835_gpio_set_pud(SENSOR_ABERTURA_CANCELA_ENTRADA, BCM2835_GPIO_PUD_DOWN);
    bcm2835_gpio_set_pud(SENSOR_FECHAMENTO_CANCELA_ENTRADA, BCM2835_GPIO_PUD_DOWN);
    bcm2835_gpio_set_pud(SENSOR_ABERTURA_CANCELA_SAIDA, BCM2835_GPIO_PUD_DOWN);
    bcm2835_gpio_set_pud(SENSOR_FECHAMENTO_CANCELA_SAIDA, BCM2835_GPIO_PUD_DOWN);
    
    // Inicializa motores das cancelas como LOW (fechadas)
    bcm2835_gpio_write(MOTOR_CANCELA_ENTRADA, LOW);
    bcm2835_gpio_write(MOTOR_CANCELA_SAIDA, LOW);
    bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO, LOW);
}

typedef struct vaga
{
    struct timeval hent;    // Horário de entrada do carro na vaga
    struct timeval hsaida;  // Horário de saída do carro da vaga
    int tempo;              // Tempo de permanência do carro na vaga
    int ncarro;             // Número do carro estacionado
    int ocupado;            // ex: Vaga 1 = 1, Vaga 2 = 2, Vaga 3 = 3, Vaga 4 = 4, Vaga livre = 0
    bool boolocupado;       // 0 = vaga livre, 1 = vaga ocupada
}vaga;

typedef struct vsoma{
    int somaValores;    // Soma dos valores das vagas ocupadas (ex: 1+2+3+4 = 10)
    int somaVagas;      // Soma das vagas ocupadas (ex: 1+1+1+1 = 4)
}vsoma;

int carro = 0;
int valor;                          // Valor lido pelo sensor de vaga
bool carroAndar = false;            // 0 = carro não está andando, 1 = carro está andando
vaga *v;
vsoma x;
int k=0, j=0, carroTotal=0;
int idoso = 1, pcd = 1, normal = 2;
int anteriorSomaValores = 0;
bool entradaManual = false;         // Controle manual de entrada via ThingsBoard
bool saidaManual = false;           // Controle manual de saída via ThingsBoard
bool entradaManualEmAndamento = false; // Flag para controlar se uma entrada manual está em andamento
bool carroPassouEntrada = false;    // Flag para detectar se o carro já passou

#define tamVetorEnviar 22
#define tamVetorReceber 5
#define tamDadosPlacar 14  // Novo: array para receber dados do placar do Central

int parametros[tamVetorEnviar];
int recebe[tamVetorReceber];
int dadosPlacar[tamDadosPlacar];  // Novo: dados do placar recebidos do Central
int fechado = 0;

// ✅ MODBUS centralizado no Térreo conforme especificação
int modbus_fd_terreo = -1;
pthread_mutex_t mutex_modbus_terreo = PTHREAD_MUTEX_INITIALIZER;

// Função para inicializar todas as vagas como vazias
void inicializarVagasTerreo(vaga *v){
    printf("Inicializando térreo - Todas as vagas vazias\n");
    for(int i = 0; i < 4; i++){
        v[i].ocupado = 0;
        v[i].boolocupado = 0;
        v[i].ncarro = 0;
        v[i].tempo = 0;
    }
    x.somaValores = 0;
    x.somaVagas = 0;
    anteriorSomaValores = 0;
    carroTotal = 0;
    printf("Térreo inicializado com sucesso - 4 vagas disponíveis\n");
}

int separaIguala(){
    
    parametros[0] = pcd;
    parametros[1] = idoso;
    parametros[2] = normal;
    parametros[3] = v[0].boolocupado;
    parametros[4] = v[1].boolocupado;
    parametros[5] = v[2].boolocupado;
    parametros[6] = v[3].boolocupado;
    parametros[12] = carroTotal;
    parametros[18] = x.somaVagas;
    fechado = recebe[1];
    parametros[19] = recebe[4];    
}

//Função que lê o sensor da cancela de entrada quando um carro está entrando no estacionamento
void * sensorEntrada(){
    while(1){
        
        // Se o estacionamento está fechado, reseta todos os flags e garante que a cancela está fechada
        if(fechado==1){
            bcm2835_gpio_write(MOTOR_CANCELA_ENTRADA, LOW);
            entradaManual = false;
            entradaManualEmAndamento = false;
            carroPassouEntrada = false;
            j = 0;
            parametros[19] = 0;
            delay(100);
            continue;
        }
        
        if(fechado==0){

        // Controle manual via ThingsBoard - Permite apenas 1 carro por comando
        if(entradaManual && !entradaManualEmAndamento){
            printf("ENTRADA MANUAL ATIVADA - Abrindo cancela de entrada\n");
            
            // === INTEGRAÇÃO LPR: Dispara captura de placa ===
            char placa[9];
            int confianca = 0;
            bool placaLida = lpr_processar_entrada(carroTotal + 1, placa, &confianca);
            
            bcm2835_gpio_write(MOTOR_CANCELA_ENTRADA, HIGH);
            parametros[19]=1;
            entradaManualEmAndamento = true; // Marca que uma operação manual está em andamento
            carroPassouEntrada = false;
            
            if(placaLida) {
                printf("Aguardando carro com placa %s (conf: %d%%) passar...\n", placa, confianca);
            } else {
                printf("Aguardando carro passar (placa não identificada)...\n");
            }
            delay(1000); // Delay para estabilizar a abertura
        }
        
        // Se entrada manual está em andamento, aguarda o carro passar
        if(entradaManualEmAndamento){
            // Detecta quando o carro passa pelo sensor de fechamento
            if(HIGH == bcm2835_gpio_lev(SENSOR_FECHAMENTO_CANCELA_ENTRADA)){
                if(!carroPassouEntrada){
                    carroPassouEntrada = true;
                    printf("Carro detectado passando pela cancela...\n");
                    
                    // Incrementa contador de carros
                    if(j==0){
                        ++carroTotal;
                        j=1;
                        parametros[19]=1;
                        printf("Carro %d entrou manualmente\n", carroTotal);
                    }
                    delay(2000); // Aguarda o carro passar completamente
                }
            }
            
            // Após o carro passar, fecha a cancela e reseta os flags
            if(carroPassouEntrada && LOW == bcm2835_gpio_lev(SENSOR_FECHAMENTO_CANCELA_ENTRADA)){
                printf("ENTRADA MANUAL - Fechando cancela de entrada\n");
                bcm2835_gpio_write(MOTOR_CANCELA_ENTRADA, LOW);
                parametros[19]=0;
                
                // Reseta todos os flags para aguardar novo comando
                entradaManual = false;
                entradaManualEmAndamento = false;
                carroPassouEntrada = false;
                j=0;
                printf("Cancela fechada. Aguardando novo comando para permitir outra entrada.\n");
                delay(1000); // Delay de segurança
            }
        }
        // Controle por sensores físicos (modo automático) - Só funciona se não houver entrada manual em andamento
        if(!entradaManual && !entradaManualEmAndamento){
            //Lê o sensor de abertura da cancela de entrada e aciona o motor da cancela para abrir
            if(HIGH == bcm2835_gpio_lev(SENSOR_ABERTURA_CANCELA_ENTRADA)){
                // === INTEGRAÇÃO LPR: Dispara captura de placa ao detectar carro ===
                char placa[9];
                int confianca = 0;
                bool placaLida = lpr_processar_entrada(carroTotal + 1, placa, &confianca);
                
                bcm2835_gpio_write(MOTOR_CANCELA_ENTRADA, HIGH);
                parametros[19]=1;
                
                if(placaLida) {
                    printf("[Entrada-Auto] Placa %s detectada (conf: %d%%)\n", placa, confianca);
                } else {
                    printf("[Entrada-Auto] Placa não identificada - ticket temporário será criado\n");
                }
                
                delay(100); // Delay para evitar detecções múltiplas
            }
            //Lê o sensor de fechamento da cancela e aciona o motor da cancela para fechar
            if(HIGH == bcm2835_gpio_lev(SENSOR_FECHAMENTO_CANCELA_ENTRADA)){
                bcm2835_gpio_write(MOTOR_CANCELA_ENTRADA, LOW);
                parametros[19]=0;
                if(j==0){
                    ++carroTotal;
                    j=1;
                    printf("Carro %d entrou automaticamente (sensor)\n", carroTotal);
                    delay(2000); // Delay maior para evitar contagem duplicada
                }
            }else {
                j=0;
            }
        }
        
        }
        
        delay(100); // Delay principal do loop para evitar execução contínua
        
    }
}

//Função que lê o sensor da cancela de saída quando um carro está saindo do estacionamento
void * sensorSaida(){
    while(1){
        
        // Controle manual via ThingsBoard
        if(saidaManual){
            printf("SAÍDA MANUAL ATIVADA - Processando saída com LPR\n");
            
            // === INTEGRAÇÃO LPR: Lê placa na saída ===
            char placa[9];
            int confianca = 0;
            bool placaLida = lpr_processar_saida(placa, &confianca);
            
            if(placaLida) {
                printf("[Saída-Manual] Placa %s identificada (conf: %d%%)\n", placa, confianca);
            } else {
                printf("[Saída-Manual] Placa não identificada\n");
            }
            
            bcm2835_gpio_write(MOTOR_CANCELA_SAIDA, HIGH);
            delay(2000); // Simula tempo de abertura da cancela
            
            printf("SAÍDA MANUAL - Fechando cancela de saída\n");
            bcm2835_gpio_write(MOTOR_CANCELA_SAIDA, LOW);
            parametros[19]=0;
            printf("Carro saiu manualmente\n");
            delay(3000); // Delay para evitar operações duplicadas
            saidaManual = false; // Reset do controle manual
        }
        // Controle por sensores físicos (modo automático)
        else {
            //Lê o sensor de abertura da cancela de saida e aciona o motor da cancela para abrir
            if(HIGH == bcm2835_gpio_lev(SENSOR_ABERTURA_CANCELA_SAIDA)){
                // === INTEGRAÇÃO LPR: Lê placa na saída (automático) ===
                char placa[9];
                int confianca = 0;
                bool placaLida = lpr_processar_saida(placa, &confianca);
                
                if(placaLida) {
                    printf("[Saída-Auto] Placa %s identificada (conf: %d%%)\n", placa, confianca);
                } else {
                    printf("[Saída-Auto] Placa não identificada\n");
                }
                
                bcm2835_gpio_write(MOTOR_CANCELA_SAIDA, HIGH);
                delay(100); // Delay para evitar detecções múltiplas
            }
            //Lê o sensor de saída da cancela de saída e aciona o motor da cancela para fechar
            if(HIGH == bcm2835_gpio_lev(SENSOR_FECHAMENTO_CANCELA_SAIDA)){
                bcm2835_gpio_write(MOTOR_CANCELA_SAIDA, LOW);
                parametros[19]=0;
                delay(100); // Delay para evitar detecções múltiplas
            }
        }
        
        delay(100); // Delay principal do loop para evitar execução contínua
    }
}

//Função que verifica quais vagas estão ocupadas
void vagasOcupadas(vaga *v){
    
    x.somaValores = 0;
    x.somaVagas = 0;
    for(int i = 0; i<4; i++){
        if(v[i].ocupado != 0){
            x.somaVagas++;
        }
        x.somaValores += v[i].ocupado;
    }
}

//Função que verifica e imprime as vagas disponíveis por tipo
void * vagasDisponiveis(vaga *v){
    // Se o estacionamento está fechado, todas as vagas são consideradas indisponíveis
    if(fechado == 1){
        idoso = 0;
        pcd = 0;
        normal = 0;
        return NULL;
    }
    
    idoso = 1;
    pcd = 1;
    normal = 2;
    
        // Vagas normais (vagas 3 e 4)
        for(int i=2; i<4; i++){
            if(v[i].ocupado > 0)
                v[i].boolocupado = 1;
            
            else if(v[i].ocupado == 0)
                v[i].boolocupado=0;
            normal -= v[i].boolocupado;
        }
        
        // Vaga idoso (vaga 2)
        if(v[1].ocupado > 0){
            v[1].boolocupado = 1;
            idoso = 0;
        }
        else if(v[1].ocupado == 0) {
            v[1].boolocupado = 0;
            idoso = 1;
        }
        
        // Vaga PCD (vaga 1)
        if(v[0].ocupado > 0){
            pcd=0;
            v[0].boolocupado = 1;
        }
        else if(v[0].ocupado == 0) {
            pcd=1;
            v[0].boolocupado=0;
        }

}

//Função que verifica a mudança de estado das vagas
int mudancaEstadoVaga(vsoma *s, int anteriorSomaValores){
    return anteriorSomaValores-s->somaValores; 
}

//Função que calcula o tempo de permanência do carro na vaga
int timediff(struct timeval entrada, struct timeval saida){
    return (int)(saida.tv_sec - entrada.tv_sec);
}

//Função que calcula o valor a ser pago pelo carro que estava na vaga
void pagamento(int g, vaga *v){
    gettimeofday(&v[g-1].hsaida,0);
    // Calcula tempo em segundos e arredonda para cima em minutos (conforme regra de negócio)
    int segundos = timediff(v[g-1].hent,v[g-1].hsaida);
    int minutos = (segundos + 59) / 60; // Arredonda para cima: qualquer fração = 1 minuto
    if(minutos < 1) minutos = 1; // Mínimo de 1 minuto (R$ 0,15)
    
    v[g-1].tempo = minutos;
    float x = minutos * 0.15;
    parametros[14]=1;
    parametros[15]=v[g-1].ncarro;
    parametros[16]=minutos;
    parametros[17]=g;
    delay(1000);
    parametros[14]=0;
}

//Função que verifica em qual vaga o carro estacionou
void buscaCarro(int f , vaga *v){
    f *= -1;
    v[f-1].ncarro = carroTotal;
    gettimeofday(&v[f-1].hent,0);
    parametros[11] = 1;
    parametros[13] = f;

    delay(1000);
    parametros[11] = 0;
}

//Função para ativar entrada manual via ThingsBoard
void ativarEntradaManual(){
    entradaManual = true;
    printf("ENTRADA MANUAL SOLICITADA VIA THINGSBOARD\n");
}

//Função para ativar saída manual via ThingsBoard
void ativarSaidaManual(){
    saidaManual = true;
    printf("SAÍDA MANUAL SOLICITADA VIA THINGSBOARD\n");
}

//Função que lê o estado das vagas do terreo
void leituraVagasTerreo(vaga *v){
    k=0;
    anteriorSomaValores = 0;        
    x.somaVagas = 0;
    x.somaValores = 0;

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
        valor = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor == 1)
            v[0].ocupado = 1;

        else if(valor == 0) 
            v[0].ocupado = 0;
        
        //Segunda vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        delay(50);
        valor = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor == 1)
            v[1].ocupado = 2;
        else if(valor == 0) 
            v[1].ocupado = 0;

        //Terceira vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        delay(50);
        valor = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor == 1)
            v[2].ocupado = 3;
        else if(valor == 0) 
            v[2].ocupado = 0;        

        //Quarta vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        delay(50);
        valor = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor == 1)
            v[3].ocupado = 4;
        else if(valor == 0) 
            v[3].ocupado = 0;        

            
        k = mudancaEstadoVaga(&x, anteriorSomaValores);
        parametros[16]=0;
        if(k>0 && k<5){
            parametros[19] = 1;
            pagamento(k, v);
        }else if(k<0 && k>-5){  
            parametros[19] = 0;
            buscaCarro(k, v);
        } 
        anteriorSomaValores = x.somaValores;
    
        if(fechado == 1) {
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO, HIGH);
        }
        else if(fechado == 0) {
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO, LOW);
        }
    }
}

void *chamaLeitura(){
    leituraVagasTerreo(v);
}

/**
 * @brief Thread que gerencia o placar MODBUS (0x20)
 * ✅ ARQUITETURA CORRETA conforme especificação:
 * - Inicializa MODBUS no Térreo (centraliza interface)
 * - Recebe comandos do Central via TCP/IP (dadosPlacar[])
 * - Escreve no placar MODBUS 0x20
 */
void *atualizaPlacarModbus() {
    // Aguarda 3 segundos para estabilizar comunicação TCP/IP com Central
    delay(3000);
    
    printf("[MODBUS-Placar-Térreo] Thread iniciada\n");
    printf("[MODBUS-Placar-Térreo] ✅ Centralizando interface MODBUS no Térreo (conforme spec)\n");
    
    // ✅ Inicializa MODBUS no Térreo (conforme orientação da especificação)
    pthread_mutex_lock(&mutex_modbus_terreo);
    modbus_fd_terreo = modbus_init(MODBUS_SERIAL_PORT);
    pthread_mutex_unlock(&mutex_modbus_terreo);
    
    if(modbus_fd_terreo < 0) {
        fprintf(stderr, "[MODBUS-Placar-Térreo] AVISO: Falha ao inicializar porta serial\n");
        fprintf(stderr, "[MODBUS-Placar-Térreo] Sistema funcionará sem atualização do placar externo\n");
        // Continua mesmo sem MODBUS (modo degradado)
    } else {
        printf("[MODBUS-Placar-Térreo] ✅ Comunicação MODBUS inicializada com sucesso\n");
        printf("[MODBUS-Placar-Térreo] ✅ Aguardando comandos do Servidor Central...\n");
    }
    
    while(1) {
        // Verifica se há comando para atualizar (dadosPlacar[13] = 1)
        if(dadosPlacar[13] == 1 && modbus_fd_terreo >= 0) {
            PlacarData placar;
            
            // ✅ Recebe dados do Central via TCP/IP (preenchidos em enviaParametros())
            placar.vagas_livres_terreo_pne = dadosPlacar[0];
            placar.vagas_livres_terreo_idoso = dadosPlacar[1];
            placar.vagas_livres_terreo_comuns = dadosPlacar[2];
            placar.vagas_livres_a1_pne = dadosPlacar[3];
            placar.vagas_livres_a1_idoso = dadosPlacar[4];
            placar.vagas_livres_a1_comuns = dadosPlacar[5];
            placar.vagas_livres_a2_pne = dadosPlacar[6];
            placar.vagas_livres_a2_idoso = dadosPlacar[7];
            placar.vagas_livres_a2_comuns = dadosPlacar[8];
            placar.num_carros_terreo = dadosPlacar[9];
            placar.num_carros_a1 = dadosPlacar[10];
            placar.num_carros_a2 = dadosPlacar[11];
            placar.flags = dadosPlacar[12];
            
            // ✅ Escreve no placar MODBUS 0x20 (conforme especificação)
            pthread_mutex_lock(&mutex_modbus_terreo);
            bool success = placar_update(modbus_fd_terreo, &placar);
            pthread_mutex_unlock(&mutex_modbus_terreo);
            
            if(!success) {
                static int erro_count = 0;
                erro_count++;
                if(erro_count % 10 == 1) {
                    fprintf(stderr, "[MODBUS-Placar-Térreo] Erro ao atualizar (total: %d erros)\n", erro_count);
                }
            }
        }
        
        // Atualiza a cada 1 segundo (conforme especificação)
        delay(1000);
    }
    
    return NULL;
}

void *enviaParametros(){
    char *ip ="127.0.0.1";
    int port = 10683;
    
    int sock;
    struct sockaddr_in addr;
    socklen_t addr_size;

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
    while(1){
        // Envia dados dos sensores ao Central
        send(sock, parametros, tamVetorEnviar * sizeof(int), 0);
        
        // Recebe comandos do Central
        recv(sock, recebe, tamVetorReceber * sizeof(int), 0);
        
        // ✅ NOVO: Recebe dados do placar MODBUS do Central
        // Conforme especificação: "Placar: sob comando do Servidor Central, escrever..."
        recv(sock, dadosPlacar, tamDadosPlacar * sizeof(int), 0);
        delay(1000);
    }
    close(sock);
    printf("Disconnected from server\n");
}

int mainT(){
    //mainT
    if (!bcm2835_init())
        return 1;

    configuraPinos();
    carro = 0;

    v = calloc(4,sizeof(vaga));
    
    // Inicializa todas as vagas como vazias
    inicializarVagasTerreo(v);
    
    // Aguarda 2 segundos para estabilizar os sensores
    printf("Aguardando estabilização dos sensores...\n");
    delay(2000);
    
    // === INICIALIZA SISTEMA LPR (MODBUS) ===
    printf("\n╔═══════════════════════════════════════════════════════╗\n");
    printf("║   INICIALIZANDO SISTEMA LPR (License Plate Reader)   ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");
    
    // Tenta inicializar LPR - se falhar, continua em modo degradado
    if(!lpr_init(MODBUS_SERIAL_PORT)) {
        printf("\n⚠️  AVISO: Sistema funcionará em MODO DEGRADADO\n");
        printf("    Todos os carros receberão tickets temporários (TEMP####)\n");
        printf("    Reconciliação manual será necessária via Central\n\n");
    } else {
        printf("\n✅ Sistema LPR operacional - Identificação automática de placas ativa\n\n");
    }
    
    delay(1000);

    pthread_t fEntrada, fSaida, fLeituraVagas, fEnviaParametros, fPlacarModbus;

    pthread_create(&fLeituraVagas, NULL, chamaLeitura, NULL);
    pthread_create(&fEnviaParametros, NULL, enviaParametros, NULL);
    pthread_create(&fEntrada,NULL,sensorEntrada,NULL);
    pthread_create(&fSaida,NULL,sensorSaida,NULL);
    // ✅ NOVO: Thread do placar MODBUS (centralizada no Térreo conforme especificação)
    pthread_create(&fPlacarModbus, NULL, atualizaPlacarModbus, NULL);

    pthread_join(fEntrada,NULL);
    pthread_join(fSaida,NULL);
    pthread_join(fLeituraVagas, NULL);
    pthread_join(fEnviaParametros, NULL);
    pthread_join(fPlacarModbus, NULL);  // ✅ Join da thread do placar

    // Cleanup LPR
    lpr_cleanup();
    
    // ✅ Cleanup MODBUS (centralizado no Térreo)
    if(modbus_fd_terreo >= 0) {
        pthread_mutex_lock(&mutex_modbus_terreo);
        modbus_close(modbus_fd_terreo);
        pthread_mutex_unlock(&mutex_modbus_terreo);
        printf("[MODBUS-Placar-Térreo] Porta serial fechada\n");
    }
    
    bcm2835_close();
    return 0;
}