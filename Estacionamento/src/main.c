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
// Protótipos das funções principais
int mainT();
int mainC();
int mainU();
int mainD();


int main(int argc, char **argv){
    (void)argc;
    char opcao = argv[1][0];

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