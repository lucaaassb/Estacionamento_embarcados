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

//ANDAR 1
#define ENDERECO_01 16                       // GPIO 16 - SAÍDA
#define ENDERECO_02 20                       // GPIO 20 - SAÍDA
#define ENDERECO_03 21                       // GPIO 21 - SAÍDA
#define SENSOR_DE_VAGA 27                    // GPIO 27 - ENTRADA
#define SENSOR_DE_PASSAGEM_1 22              // GPIO 22 - ENTRADA
#define SENSOR_DE_PASSAGEM_2 11              // GPIO 11 - ENTRADA
#define SINAL_DE_LOTADO_FECHADO1 RPI_GPIO_P1_24             // PINO 08 - SAÍDA

void configuraPinos1(){
    bcm2835_gpio_fsel(ENDERECO_01, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(ENDERECO_02, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(ENDERECO_03, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_DE_VAGA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SINAL_DE_LOTADO_FECHADO1, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_DE_PASSAGEM_1, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SENSOR_DE_PASSAGEM_2, BCM2835_GPIO_FSEL_INPT);
    
    // Configura pull-down nos sensores de vaga para evitar leituras falsas
    bcm2835_gpio_set_pud(SENSOR_DE_VAGA, BCM2835_GPIO_PUD_DOWN);
    bcm2835_gpio_set_pud(SENSOR_DE_PASSAGEM_1, BCM2835_GPIO_PUD_DOWN);
    bcm2835_gpio_set_pud(SENSOR_DE_PASSAGEM_2, BCM2835_GPIO_PUD_DOWN);
    
    // Inicializa sinal de lotado como LOW (não lotado)
    bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO1, LOW);
}

typedef struct vaga
{
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

vaga *a;
vsoma s;
int valor1;                          // Valor lido pelo sensor de vaga
bool carroAndar1 = false;            // 0 = carro não está andando, 1 = carro está andando  
int k1=0, carroTotal1=0;
int idoso1 = 2, pcd1 = 1, normal1 = 5, fechado1 = 0;
int anteriorSomaValores1 = 0;

#define tamVetorEnviar 23
#define tamVetorReceber 5

int parametros1[tamVetorEnviar];
int recebe1[tamVetorReceber];

// Função para inicializar todas as vagas como vazias
void inicializarVagas1(vaga *v){
    printf("Inicializando 1º andar - Todas as vagas vazias\n");
    for(int i = 0; i < 8; i++){
        v[i].ocupado = 0;
        v[i].boolocupado = 0;
        v[i].ncarro = 0;
        v[i].tempo = 0;
    }
    s.somaValores = 0;
    s.somaVagas = 0;
    anteriorSomaValores1 = 0;
    printf("1º andar inicializado com sucesso - 8 vagas disponíveis\n");
}

int separaIguala1(){
    
    parametros1[0]  = pcd1;
    parametros1[1]  = idoso1;
    parametros1[2]  = normal1;
    parametros1[3]  = a[0].boolocupado;
    parametros1[4]  = a[1].boolocupado;
    parametros1[5]  = a[2].boolocupado;
    parametros1[6]  = a[3].boolocupado;
    parametros1[7]  = a[4].boolocupado;
    parametros1[8]  = a[5].boolocupado;
    parametros1[9]  = a[6].boolocupado;
    parametros1[10] = a[7].boolocupado;
    parametros1[12]= recebe1[0];
    parametros1[18]= s.somaVagas;
    fechado1 = recebe1[2];
}

void vagasOcupadas1(vaga *v){
    
    s.somaValores = 0;
    s.somaVagas = 0;
    for(int i = 0; i<8; i++){
        if(v[i].ocupado != 0){
            s.somaVagas++;
        }
        s.somaValores += v[i].ocupado;
    }
    
}

void * vagasDisponiveis1(vaga *p){
    // Se o 1º andar está fechado, todas as vagas são consideradas indisponíveis
    if(fechado1 == 1){
        idoso1 = 0;
        pcd1 = 0;
        normal1 = 0;
        return NULL;
    }
    
    idoso1 = 2;
    pcd1 = 1;
    normal1 = 5;
    
        for(int i=3; i<8; i++){
            if(p[i].ocupado > 0)
                p[i].boolocupado = 1;
            
            else if(p[i].ocupado == 0)
                p[i].boolocupado=0;
            normal1 -= p[i].boolocupado;
        }
        for(int i=1; i<3; i++){
            if(p[i].ocupado > 0)
                p[i].boolocupado = 1;
            
            else if(p[i].ocupado == 0)
                p[i].boolocupado=0;

            idoso1 -= p[i].boolocupado;
        }
        
        if(p[0].ocupado > 0){
            pcd1=0;
            p[0].boolocupado = 1;
        }
        else if(p[0].ocupado == 0) {
            pcd1=1;
            p[0].boolocupado = 0;
        }
}

int mudancaEstadoVaga1( int anteriorSomaValores1){
    return anteriorSomaValores1-s.somaValores; 
}

int timediff1(struct timeval entrada, struct timeval saida){ 
    return (int)(saida.tv_sec - entrada.tv_sec);
}

void pagamento1(int g, vaga *a){
    
    gettimeofday(&a[g-1].hsaida,0);
    a[g-1].tempo = timediff1(a[g-1].hent,a[g-1].hsaida)/60;
    float f = (a[g-1].tempo*0.15);
    parametros1[14]=1;
    parametros1[15]=a[g-1].ncarro;
    parametros1[16]=a[g-1].tempo;
    parametros1[17]=g;
    delay(1000);
    parametros1[14]=0;
}

void buscaCarro1(int f , vaga *a){
    f *= -1;
    a[f-1].ncarro = parametros1[12];
    gettimeofday(&a[f-1].hent,0);
    parametros1[11] = 1;
    parametros1[13] = f;
    delay(1000);
    parametros1[11] = 0;
}

void leituraVagasAndar1(vaga *b){
    k1=0;       
    s.somaVagas = 0;
    s.somaValores = 0;

    while(1){
        
        delay(50);
        vagasOcupadas1(a);
        vagasDisponiveis1(a);
        separaIguala1();

        //Primeira vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor1 == 1)
            b[0].ocupado = 1;
            
        else if(valor1 == 0) 
            b[0].ocupado = 0;
        
        //Segunda vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor1 == 1)
            b[1].ocupado = 2;
        else if(valor1 == 0) 
            b[1].ocupado = 0;

        //Terceira vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor1 == 1)
            b[2].ocupado = 3;
        else if(valor1 == 0) 
            b[2].ocupado = 0;        

        //Quarta vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, LOW);
        delay(50);
        valor1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor1 == 1)
            b[3].ocupado = 4;
        else if(valor1 == 0) 
            b[3].ocupado = 0;        

        //Quinta vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor1 == 1)
            b[4].ocupado = 5;
        else if(valor1 == 0) 
            b[4].ocupado = 0;        

        //Sexta vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, LOW);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor1 == 1)
            b[5].ocupado = 6;
        else if(valor1 == 0) 
            b[5].ocupado = 0;        

        //Sétima vaga
        bcm2835_gpio_write(ENDERECO_01, LOW);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor1 == 1)
            b[6].ocupado = 7;
        else if(valor1 == 0) 
            b[6].ocupado = 0;

        //Oitava vaga
        bcm2835_gpio_write(ENDERECO_01, HIGH);
        bcm2835_gpio_write(ENDERECO_02, HIGH);
        bcm2835_gpio_write(ENDERECO_03, HIGH);
        delay(50);
        valor1 = bcm2835_gpio_lev(SENSOR_DE_VAGA);
        if(valor1 == 1)
            b[7].ocupado = 8;
        else if(valor1 == 0) 
            b[7].ocupado = 0;

//-------------------------------------------------------------//
        k1 = mudancaEstadoVaga1(anteriorSomaValores1);
        if(k1>0 && k1<9){
            pagamento1(k1, b);
            
        }else if(k1<0 && k1>-9){
            buscaCarro1(k1, b);
        } 
        anteriorSomaValores1 = s.somaValores;
        
        if((s.somaVagas < 8 && fechado1==0)){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO1, LOW);
            parametros1[20] = 0;
        }
        else if(s.somaVagas==8){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO1, HIGH);
            parametros1[20] = 1;
        } 
        
        else if(fechado1==1){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO1, HIGH);
            parametros1[20] = 1;
        } 
        else if(fechado1 == 0){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO1, LOW);
            parametros1[20] = 0;
        } 
    }
}

void *chamaLeitura1(){
    leituraVagasAndar1(a);
}

void *enviaParametros1(){
    char *ip ="127.0.0.1";
    int port = 10681;
    
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
        send (sock, parametros1, tamVetorEnviar *sizeof(int) , 0);
        recv(sock, recebe1, tamVetorReceber * sizeof(int), 0);
        delay(1000);
    }
    close(sock);
    printf("Disconnected from server\n");
}

int mainU(){
    //mainU
    
    if (!bcm2835_init())
        return 1;
    
    configuraPinos1();

    a = calloc(8,sizeof(vaga));
    
    // Inicializa todas as vagas como vazias
    inicializarVagas1(a);
    
    // Aguarda 2 segundos para estabilizar os sensores
    printf("Aguardando estabilização dos sensores...\n");
    delay(2000);
    
    pthread_t fLeituraVagas1, fEnviaParametros1, fSensorPassagemA;
    
    pthread_create(&fLeituraVagas1, NULL, chamaLeitura1, NULL);
    pthread_create(&fEnviaParametros1, NULL, enviaParametros1, NULL);
    //pthread_create(&fSensorPassagemA, NULL, sensorPassagemA, NULL);
    pthread_join(fLeituraVagas1, NULL);
    pthread_join(fEnviaParametros1, NULL);
    //pthread_join(fSensorPassagemA, NULL);
    bcm2835_close();
    return 0;
}