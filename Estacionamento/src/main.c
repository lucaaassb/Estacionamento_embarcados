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
#include <sys/types.h>
#include "terreo.h"
#include "central.h"
#include "andar1.h"
#include "andar2.h"


int main(int argc, char **argv){

    char opcao = argv[1][0];
    pthread_t aux1,aux2,aux3,aux4;

    if(opcao == 't'){
        mainT();
    }

    if(opcao== 'c'){
        mainC();
    }

    if(opcao== 'u'){
        mainU();
    }

    if(opcao== 'd'){
        mainD();
    }

    return 0;
}