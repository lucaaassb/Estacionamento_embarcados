#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "inc/terreo.h"

// Programa de teste para demonstrar controle manual via ThingsBoard
int main(){
    printf("=== TESTE DE CONTROLE MANUAL VIA THINGSBOARD ===\n");
    printf("Este programa simula cliques no ThingsBoard\n\n");
    
    int opcao;
    
    while(1){
        printf("\nEscolha uma opção:\n");
        printf("1 - Simular clique em 'Entrada' no ThingsBoard\n");
        printf("2 - Simular clique em 'Saída' no ThingsBoard\n");
        printf("3 - Sair\n");
        printf("Opção: ");
        
        scanf("%d", &opcao);
        
        switch(opcao){
            case 1:
                printf("\n>>> SIMULANDO CLIQUE EM 'ENTRADA' NO THINGSBOARD <<<\n");
                ativarEntradaManual();
                printf("Aguarde 5 segundos para ver o resultado...\n");
                sleep(5);
                break;
                
            case 2:
                printf("\n>>> SIMULANDO CLIQUE EM 'SAÍDA' NO THINGSBOARD <<<\n");
                ativarSaidaManual();
                printf("Aguarde 5 segundos para ver o resultado...\n");
                sleep(5);
                break;
                
            case 3:
                printf("Saindo...\n");
                return 0;
                
            default:
                printf("Opção inválida!\n");
                break;
        }
    }
    
    return 0;
}
