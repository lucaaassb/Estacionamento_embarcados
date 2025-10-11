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
#define ENDERECO_01 RPI_V2_GPIO_P1_21                       // PINO 09 - SAÍDA
#define ENDERECO_02 RPI_GPIO_P1_23                          // PINO 11 - SAÍDA
#define ENDERECO_03 RPI_GPIO_P1_10                          // PINO 15 - SAÍDA
#define SENSOR_DE_VAGA RPI_V2_GPIO_P5_03                    // PINO 01 - ENTRADA
#define SENSOR_DE_PASSAGEM_1 RPI_GPIO_P1_26 //RPI_V2_GPIO_P1_13  //pino 00
#define SENSOR_DE_PASSAGEM_2 RPI_GPIO_P1_26                 // PINO 07 - ENTRADA
#define SINAL_DE_LOTADO_FECHADO2 RPI_V2_GPIO_P1_08          // PINO 14 - SAÍDA

void configuraPinos2(){
    bcm2835_gpio_fsel(ENDERECO_01, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(ENDERECO_02, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(ENDERECO_03, BCM2835_GPIO_FSEL_OUTP);
    //bcm2835_gpio_fsel(SENSOR_DE_VAGA, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SINAL_DE_LOTADO_FECHADO2, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SENSOR_DE_PASSAGEM_1, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(SENSOR_DE_PASSAGEM_2, BCM2835_GPIO_FSEL_INPT);
    
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
int idoso2 = 2, pcd2 = 2, normal2 = 5;
int anteriorSomaValores2 = 0;
int parametros2[20];
int recebe2[4], fechado2=0;

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
    fechado2=recebe2[3];
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
    a[g-1].tempo = timediff2(a[g-1].hent,a[g-1].hsaida)/60;
    float f = (a[g-1].tempo*0.1);
    parametros2[14]=1;
    parametros2[15]=a[g-1].ncarro;
    parametros2[16]=a[g-1].tempo;
    parametros2[17]=g;
    delay(1000);
    parametros2[14]=0;
}

void buscaCarro2(int f , vaga *a){
    f *= -1;
    a[f-1].ncarro = carroTotal2;
    gettimeofday(&a[f-1].hent,0);
    parametros2[11] = 1;
    parametros2[12] = carroTotal2;
    parametros2[13] = f;
    delay(1000);
    parametros2[11] = 0;
}

void leituraVagasTerreo2(vaga *b){
    k2=0;       
    t.somaVagas = 0;
    t.somaValores = 0;

    while(1){
        system("clear");
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
        printf("Vaga 1: %d\n", b[0].ocupado);
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
        printf("Vaga 2: %d\n", b[1].ocupado);
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
        printf("Vaga 3: %d\n", b[2].ocupado);
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
        printf("Vaga 4: %d\n", b[3].ocupado);
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
        printf("Vaga 5: %d\n", b[4].ocupado);
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
        printf("Vaga 6: %d\n", b[5].ocupado);
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
        printf("Vaga 7: %d\n", b[6].ocupado);

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
        printf("Vaga 8: %d\n", b[7].ocupado);
//-------------------------------------------------------------//
        k2 = mudancaEstadoVaga2(anteriorSomaValores2);
        if(k2>0 && k2<9){
            pagamento2(k2, b);
            
        }else if(k2<0 && k2>-9){       
            buscaCarro2(k2, b);
        } 
        anteriorSomaValores2 = t.somaValores;
        
        if(t.somaVagas < 8 && fechado2 == 0)bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO2, LOW);
        else if(t.somaVagas==8) bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO2, HIGH);
        if(fechado2 == 1){
            bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO2, HIGH);
        }
        else if(fechado2 == 0) bcm2835_gpio_write(SINAL_DE_LOTADO_FECHADO2, LOW);
        printf("Vagas disponiveis: %d\n", t.somaVagas);
        delay(1000);
    }
}

void *chamaLeitura2(){
    leituraVagasTerreo2(b);
}
/*
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
        send (sock, parametros2, 20*sizeof(int) , 0);
        recv(sock, recebe2, 4 * sizeof(int), 0);
        printf("Fechado: %d\n", recebe2[3]);
        delay(1000);
    }
    close(sock);
    printf("Disconnected from server\n");
}
*/
int main(){
   if (!bcm2835_init())
        return 1;
    configuraPinos2();
    
    b = calloc(8,sizeof(vaga));
    
    pthread_t fLeituraVagas2, fEnviaParametros2;
    
    pthread_create(&fLeituraVagas2, NULL, chamaLeitura2, NULL);
    //pthread_create(&fEnviaParametros2, NULL, enviaParametros2, NULL);
    
    pthread_join(fLeituraVagas2, NULL);
    //pthread_join(fEnviaParametros2, NULL);

    bcm2835_close();
    return 0;
}
