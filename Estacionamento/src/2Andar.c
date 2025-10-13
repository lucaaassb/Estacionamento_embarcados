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


//ANDAR 2
// Usando números GPIO BCM diretamente (alguns pinos físicos não têm constantes na biblioteca)
#define ENDERECO_01 0                                       // GPIO 0 - Pino físico 27 - SAÍDA
#define ENDERECO_02 5                                       // GPIO 5 - Pino físico 29 - SAÍDA
#define ENDERECO_03 6                                       // GPIO 6 - Pino físico 31 - SAÍDA
#define SENSOR_DE_VAGA 13                                   // GPIO 13 - Pino físico 33 - ENTRADA
#define SENSOR_DE_PASSAGEM_1 19                             // GPIO 19 - Pino físico 35 - ENTRADA
#define SENSOR_DE_PASSAGEM_2 26                             // GPIO 26 - Pino físico 37 - ENTRADA
#define SINAL_DE_LOTADO_FECHADO2 RPI_V2_GPIO_P1_08          // GPIO 14 - Pino físico 8 - SAÍDA

void configuraPinos2(){
    bcm2835_gpio_fsel(ENDERECO_01, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(ENDERECO_02, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(ENDERECO_03, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_DE_VAGA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SINAL_DE_LOTADO_FECHADO2, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_DE_PASSAGEM_1, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SENSOR_DE_PASSAGEM_2, BCM2835_GPIO_FSEL_INPT);
    
    // Configura pull-down nos sensores de vaga para evitar leituras falsas
    bcm2835_gpio_set_pud(SENSOR_DE_VAGA, BCM2835_GPIO_PUD_DOWN);
    bcm2835_gpio_set_pud(SENSOR_DE_PASSAGEM_1, BCM2835_GPIO_PUD_DOWN);
    bcm2835_gpio_set_pud(SENSOR_DE_PASSAGEM_2, BCM2835_GPIO_PUD_DOWN);
    
    // Inicializa sinal de lotado como LOW (não lotado)
    bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO2, LOW);
}
/*
parametros[0] = vagas disponiveis pcd;       parametros[10] = v[7].ocupado;
parametros[1] = vagas disponiveis idoso;     parametros[11] = bool carro entrando;
parametros[2] = vagas disponiveis regular;   parametros[12] = numero carro entrando ;
parametros[3] = v[0].ocupado;                parametros[13] = vaga estacionada carro entrando;
parametros[4] = v[1].ocupado;                parametros[14] = bool carro saindo;
parametros[4] = v[2].ocupado;                parametros[15] = numero do carro saindo;
parametros[5] = v[3].ocupado;                parametros[16] = tempo de permanencia do carro saindo;
parametros[6] = v[4].ocupado;                parametros[17] = ;
parametros[7] = v[5].ocupado;                parametros[18] =  
parametros[8] = v[6].ocupado;                parametros[19] = 
*/

typedef struct vaga{
    struct timeval hent;    // Horário de entrada do carro na vaga
    struct timeval hsaida;  // Horário de saída do carro da vaga
    int tempo;              // Tempo de permanência do carro na vaga
    int ncarro;             // Número do carro estacionado
    int ocupado;            // ex: Vaga 1 = 1, Vaga 2 = 2, Vaga 3 = 3, Vaga 4 = 4, Vaga 5 = 5, Vaga 6 = 6, Vaga 7 = 7, Vaga 8 = 8, Vaga livre = 0
    bool boolocupado;       // 0 = vaga livre, 1 = vaga ocupada
}vaga;
typedef struct vsoma{
    int somaValores;    // Soma dos valores das vagas ocupadas (ex: 1+2+3+4+5+6+7+8 = 36)
    int somaVagas;      // Soma das vagas ocupadas (ex: 1+1+1+1+1+1+1+1 = 8)
}vsoma;

vaga *b;
vsoma t;
int valor2;                          // Valor lido pelo sensor de vaga
bool carroAndar2 = false;            // 0 = carro não está andando, 1 = carro está andando  
int k2=0, j2=0, carroTotal2=0;
int idoso2 = 2, pcd2 = 2, normal2 = 5, fechado2=0;
int anteriorSomaValores2 = 0;

#define tamVetorEnviar 23
#define tamVetorReceber 5

int parametros2[tamVetorEnviar];
int recebe2[tamVetorReceber];

// Função para inicializar todas as vagas como vazias
void inicializarVagas2(vaga *v){
    printf("Inicializando 2º andar - Todas as vagas vazias\n");
    for(int i = 0; i < 8; i++){
        v[i].ocupado = 0;
        v[i].boolocupado = 0;
        v[i].ncarro = 0;
        v[i].tempo = 0;
    }
    t.somaValores = 0;
    t.somaVagas = 0;
    anteriorSomaValores2 = 0;
    printf("2º andar inicializado com sucesso - 8 vagas disponíveis\n");
}

int separaIguala2(){
    
    parametros2[0]  = pcd2;
    parametros2[1]  = idoso2;
    parametros2[2]  = normal2;
    parametros2[3]  = b[0].boolocupado;
    parametros2[4]  = b[1].boolocupado;
    parametros2[5]  = b[2].boolocupado;
    parametros2[6]  = b[3].boolocupado;
    parametros2[7]  = b[4].boolocupado;
    parametros2[8]  = b[5].boolocupado;
    parametros2[9]  = b[6].boolocupado;
    parametros2[10] = b[7].boolocupado;
    parametros2[12] = recebe2[0];
    parametros2[18] = t.somaVagas;
    fechado2 = recebe2[3];
}

void vagasOcupadas2(vaga *v){
    
    t.somaValores = 0;
    t.somaVagas = 0;
    for(int i = 0; i<8; i++){
        if(v[i].ocupado != 0){
            t.somaVagas++;
        }
        t.somaValores += v[i].ocupado;
    }
}

void * vagasDisponiveis2(vaga *p){
    // Se o 2º andar está fechado, todas as vagas são consideradas indisponíveis
    if(fechado2 == 1){
        idoso2 = 0;
        pcd2 = 0;
        normal2 = 0;
        return NULL;
    }
    
    idoso2 = 2;
    pcd2 = 1;
    normal2 = 5;
    
        for(int i=3; i<8; i++){
            if(p[i].ocupado > 0)
                p[i].boolocupado = 1;
            
            else if(p[i].ocupado == 0)
                p[i].boolocupado=0;
            normal2 -= p[i].boolocupado;
        }
        for(int i=1; i<3; i++){
            if(p[i].ocupado > 0)
                p[i].boolocupado = 1;
            
            else if(p[i].ocupado == 0)
                p[i].boolocupado=0;
            
            idoso2 -= p[i].boolocupado;
        }
        
        if(p[0].ocupado > 0){
            pcd2=0;
            p[0].boolocupado = 1;
        }
        else if(p[0].ocupado == 0) {
            pcd2=1;
            p[0].boolocupado = 0;
        }
}

int mudancaEstadoVaga2( int anteriorSomaValores1){
    return anteriorSomaValores2-t.somaValores; 
}

int timediff2(struct timeval entrada, struct timeval saida){ 
    return (int)(saida.tv_sec - entrada.tv_sec);
}

void pagamento2(int g, vaga *a){
    
    gettimeofday(&a[g-1].hsaida,0);
    // Calcula tempo em segundos e arredonda para cima em minutos
    int segundos = timediff2(a[g-1].hent,a[g-1].hsaida);
    int minutos = (segundos + 59) / 60; // Arredonda para cima: qualquer fração = 1 minuto
    if(minutos < 1) minutos = 1; // Mínimo de 1 minuto (R$ 0,15)
    
    a[g-1].tempo = minutos;
    float f = (minutos * 0.15);
    parametros2[14]=1;
    parametros2[15]=a[g-1].ncarro;
    parametros2[16]=minutos;
    parametros2[17]=g;
    delay(100);  // ✅ REDUZIDO: 100ms para atualização mais rápida
    parametros2[14]=0;
}

void buscaCarro2(int f , vaga *a){
    f *= -1;
    a[f-1].ncarro = parametros2[12];
    gettimeofday(&a[f-1].hent,0);
    parametros2[11] = 1;
    parametros2[13] = f;
    delay(1000);
    parametros2[11] = 0;
}

void leituraVagasAndar2(vaga *b){
    k2=0;       
    t.somaVagas = 0;
    t.somaValores = 0;

    while(1){
    
        delay(50);
        vagasOcupadas2(b);
        vagasDisponiveis2(b);
        separaIguala2();
        //Primeira vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor2 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor2 == 1)
            b[0].ocupado = 1;
            
        else if(valor2 == 0) 
            b[0].ocupado = 0;
        
        //Segunda vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor2 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor2 == 1)
            b[1].ocupado = 2;
        else if(valor2 == 0) 
            b[1].ocupado = 0;

        //Terceira vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor2 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor2 == 1)
            b[2].ocupado = 3;
        else if(valor2 == 0) 
            b[2].ocupado = 0;        

        //Quarta vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor2 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor2 == 1)
            b[3].ocupado = 4;
        else if(valor2 == 0) 
            b[3].ocupado = 0;        

        //Quinta vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor2 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor2 == 1)
            b[4].ocupado = 5;
        else if(valor2 == 0) 
            b[4].ocupado = 0;        

        //Sexta vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor2 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor2 == 1)
            b[5].ocupado = 6;
        else if(valor2 == 0) 
            b[5].ocupado = 0;        

        //Sétima vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor2 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor2 == 1)
            b[6].ocupado = 7;
        else if(valor2 == 0) 
            b[6].ocupado = 0;

        //Oitava vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor2 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor2 == 1)
            b[7].ocupado = 8;
        else if(valor2 == 0) 
            b[7].ocupado = 0;

//-------------------------------------------------------------//
        k2 = mudancaEstadoVaga2(anteriorSomaValores2);
        if(k2>0 && k2<9){
            pagamento2(k2, b);
            
        }else if(k2<0 && k2>-9){
            // ✅ BLOQUEIO: Só permite estacionar se o andar NÃO está fechado
            if(fechado2 == 0){
                buscaCarro2(k2, b);
            } else {
                printf("[2º Andar] 🚫 Vaga %d IGNORADA - Andar está bloqueado\n", k2 * -1);
                // Reseta a vaga para evitar registro fantasma
                b[(k2 * -1) - 1].ocupado = 0;
            }
        } 
        anteriorSomaValores2 = t.somaValores;
        
        if(t.somaVagas < 8 && fechado2 == 0){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO2, LOW);
            parametros2[20] = 0;
        }
        else if(t.somaVagas==8){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO2, HIGH);
            parametros2[20] = 1;
        } 
        else if(fechado2 == 1){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO2, HIGH);
            parametros2[20] = 1;
        } 
        else if(fechado2 == 0 ) {
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO2, LOW);
            parametros2[20] = 0;
        }
    }
}

void *chamaLeitura2(){
    leituraVagasAndar2(b);
}

/**
 * @brief Thread que detecta passagem de carros entre andares no 2º Andar
 * 
 * Lógica de detecção:
 * - SENSOR_1 ativado primeiro, depois SENSOR_2: Carro SUBINDO (1º Andar → 2º Andar)
 * - SENSOR_2 ativado primeiro, depois SENSOR_1: Carro DESCENDO (2º Andar → 1º Andar)
 * 
 * Registra no array parametros2[21] e parametros2[22]:
 * - parametros2[21] = direção (1 = subindo, 2 = descendo)
 * - parametros2[22] = flag de evento (1 = passagem detectada, 0 = sem evento)
 */
void *sensorPassagemB(){
    printf("[2º Andar] Thread de detecção de passagem iniciada\n");
    
    int sensor1_anterior = 0;
    int sensor2_anterior = 0;
    int sensor1_ativo = 0;
    int sensor2_ativo = 0;
    int primeiro_sensor = 0;  // 1 = sensor1 ativou primeiro, 2 = sensor2 ativou primeiro
    
    while(1){
        // Lê os sensores
        sensor1_ativo = bcm2835_gpio_lev(SENSOR_DE_PASSAGEM_1);
        sensor2_ativo = bcm2835_gpio_lev(SENSOR_DE_PASSAGEM_2);
        
        // Detecta qual sensor ativou primeiro (borda de subida)
        if(sensor1_ativo == 1 && sensor1_anterior == 0 && primeiro_sensor == 0){
            // Sensor 1 ativou primeiro
            primeiro_sensor = 1;
            printf("[2º Andar] Sensor 1 detectou presença\n");
        }
        else if(sensor2_ativo == 1 && sensor2_anterior == 0 && primeiro_sensor == 0){
            // Sensor 2 ativou primeiro
            primeiro_sensor = 2;
            printf("[2º Andar] Sensor 2 detectou presença\n");
        }
        
        // Detecta a direção completa
        if(primeiro_sensor == 1 && sensor2_ativo == 1){
            // ✅ BLOQUEIO: Verifica se o 2º andar está fechado ANTES de permitir subida
            if(fechado2 == 1){
                printf("[2º Andar] 🚫 BLOQUEADO - Impedindo subida (andar fechado)\n");
                primeiro_sensor = 0;  // Reseta para evitar loop
                delay(1000);  // Aguarda carro retornar
            } else {
                // Sensor 1 → Sensor 2: Carro SUBINDO (1º Andar → 2º Andar)
                printf("[2º Andar] ↑ SUBINDO: 1º Andar → 2º Andar\n");
                parametros2[21] = 1;  // 1 = subindo
                parametros2[22] = 1;  // Flag de evento
                
                delay(1000);  // Aguarda passagem completa
                parametros2[22] = 0;  // Reseta flag
                primeiro_sensor = 0;
            }
        }
        else if(primeiro_sensor == 2 && sensor1_ativo == 1){
            // Sensor 2 → Sensor 1: Carro DESCENDO (2º Andar → 1º Andar)
            printf("[2º Andar] ↓ DESCENDO: 2º Andar → 1º Andar\n");
            parametros2[21] = 2;  // 2 = descendo
            parametros2[22] = 1;  // Flag de evento
            
            delay(1000);  // Aguarda passagem completa
            parametros2[22] = 0;  // Reseta flag
            primeiro_sensor = 0;
        }
        
        // Reseta se ambos os sensores desativarem
        if(sensor1_ativo == 0 && sensor2_ativo == 0){
            primeiro_sensor = 0;
        }
        
        // Atualiza estados anteriores
        sensor1_anterior = sensor1_ativo;
        sensor2_anterior = sensor2_ativo;
        
        delay(50);  // Polling rápido para não perder eventos
    }
    
    return NULL;
}

void *enviaParametros2(){
    char *ip ="127.0.0.1";
    int port = 10682;
    
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
        send (sock, parametros2, tamVetorEnviar *sizeof(int) , 0);
        recv(sock, recebe2, tamVetorReceber * sizeof(int), 0);
        delay(1000);
    }
    close(sock);
    printf("Disconnected from server\n");
}

int mainD(){
   if (!bcm2835_init())
        return 1;
    configuraPinos2();
    
    b = calloc(8,sizeof(vaga));
    
    // Inicializa todas as vagas como vazias
    inicializarVagas2(b);
    
    // Aguarda 2 segundos para estabilizar os sensores
    printf("Aguardando estabilização dos sensores...\n");
    delay(2000);
    
    pthread_t fLeituraVagas2, fEnviaParametros2, fPassagemAndar2;
    
    pthread_create(&fLeituraVagas2, NULL, chamaLeitura2, NULL);
    pthread_create(&fEnviaParametros2, NULL, enviaParametros2, NULL);
    pthread_create(&fPassagemAndar2, NULL, sensorPassagemB, NULL);  // ✅ Ativado
    
    pthread_join(fLeituraVagas2, NULL);
    pthread_join(fEnviaParametros2, NULL);
    pthread_join(fPassagemAndar2, NULL);  // ✅ Ativado

    bcm2835_close();
    return 0;
}
