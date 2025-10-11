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
#include <ctype.h>

#define tamVetorReceber 23
#define tamVetorEnviar 5
#define MAX_CARROS 20  // Capacidade máxima do estacionamento

int terreo[tamVetorReceber];
int andar1[tamVetorReceber];
int andar2[tamVetorReceber];
int enviar[tamVetorEnviar];
int r = 0;
int manual =  0;

// Estrutura para rastrear cada carro no estacionamento
typedef struct {
    int numero;           // Número do carro
    int andar;            // 0=Térreo, 1=1ºAndar, 2=2ºAndar, -1=Vazio
    int vaga;             // Número da vaga (1-4 térreo, 1-8 andares)
    time_t timestamp;     // Hora de entrada
    bool ativo;           // true=estacionado, false=slot vazio
} CarroEstacionado;

// Array global para rastrear todos os carros
CarroEstacionado carros[MAX_CARROS];
pthread_mutex_t mutex_carros = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Verifica se há uma tecla pressionada no terminal (non-blocking)
 * @return 1 se houver tecla pressionada, 0 caso contrário
 */
int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;
    
    // Salva configurações atuais do terminal
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    
    // Desabilita modo canônico e echo
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    // Configura stdin como non-blocking
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    // Tenta ler um caractere
    ch = getchar();
    
    // Restaura configurações originais do terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    // Verifica se uma tecla foi pressionada
    if (ch != EOF) {
        ungetc(ch, stdin);  // Devolve o caractere para o buffer
        return 1;
    }
    
    return 0;
}

/**
 * @brief Limpa o buffer de entrada (stdin) para evitar leituras indesejadas
 */
void limparBuffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/**
 * @brief Registra evento no log do sistema
 */
void registrarEvento(const char *evento) {
    FILE *log = fopen("estacionamento_log.txt", "a");
    if(log) {
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(log, "[%s] %s\n", buffer, evento);
        fclose(log);
    }
}

/**
 * @brief Inicializa o sistema de rastreamento de carros
 */
void inicializarRastreamentoCarros() {
    pthread_mutex_lock(&mutex_carros);
    for(int i = 0; i < MAX_CARROS; i++) {
        carros[i].numero = 0;
        carros[i].andar = -1;
        carros[i].vaga = 0;
        carros[i].timestamp = 0;
        carros[i].ativo = false;
    }
    pthread_mutex_unlock(&mutex_carros);
    printf("[Sistema] Rastreamento de carros inicializado\n");
    registrarEvento("🚀 SISTEMA INICIADO - Rastreamento ativo");
}

/**
 * @brief Adiciona um carro ao sistema de rastreamento com log
 * @param numeroCarro Número do carro
 * @param andar Andar onde está (0=Térreo, 1=1ºAndar, 2=2ºAndar)
 * @param vaga Número da vaga
 * @return true se adicionado com sucesso, false se não houver espaço
 */
bool adicionarCarro(int numeroCarro, int andar, int vaga) {
    pthread_mutex_lock(&mutex_carros);
    
    // Verifica se o carro já está no sistema (prevenção de duplicatas)
    for(int i = 0; i < MAX_CARROS; i++) {
        if(carros[i].ativo && carros[i].numero == numeroCarro) {
            pthread_mutex_unlock(&mutex_carros);
            printf("[Rastreamento] AVISO: Carro %d já está registrado!\n", numeroCarro);
            return false; // Previne duplicata
        }
    }
    
    // Busca um slot vazio
    for(int i = 0; i < MAX_CARROS; i++) {
        if(!carros[i].ativo) {
            carros[i].numero = numeroCarro;
            carros[i].andar = andar;
            carros[i].vaga = vaga;
            carros[i].timestamp = time(NULL);
            carros[i].ativo = true;
            pthread_mutex_unlock(&mutex_carros);
            
            char andarNome[15];
            if(andar == 0) sprintf(andarNome, "Térreo");
            else if(andar == 1) sprintf(andarNome, "1º Andar");
            else sprintf(andarNome, "2º Andar");
            
            printf("[Rastreamento] Carro %d adicionado → %s vaga %d\n", 
                   numeroCarro, andarNome, vaga);
            
            // Log para arquivo
            FILE *log = fopen("estacionamento_log.txt", "a");
            if(log) {
                time_t t = time(NULL);
                struct tm *tm_info = localtime(&t);
                char buffer[64];
                strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
                fprintf(log, "[%s] ENTRADA - Carro %d → %s vaga %d\n", 
                        buffer, numeroCarro, andarNome, vaga);
                fclose(log);
            }
            
            return true;
        }
    }
    
    pthread_mutex_unlock(&mutex_carros);
    printf("[Rastreamento] ERRO: Capacidade máxima atingida!\n");
    
    // Log do erro
    FILE *log = fopen("estacionamento_log.txt", "a");
    if(log) {
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(log, "[%s] ERRO - Capacidade máxima! Carro %d não pode entrar\n", 
                buffer, numeroCarro);
        fclose(log);
    }
    
    return false;
}

/**
 * @brief Remove um carro do sistema de rastreamento com validação de auditoria
 * @param numeroCarro Número do carro a remover
 * @return true se removido com sucesso, false se não encontrado
 */
bool removerCarro(int numeroCarro) {
    pthread_mutex_lock(&mutex_carros);
    
    for(int i = 0; i < MAX_CARROS; i++) {
        if(carros[i].ativo && carros[i].numero == numeroCarro) {
            // Calcula tempo e valor para o log
            time_t agora = time(NULL);
            int segundos = (int)difftime(agora, carros[i].timestamp);
            int minutos = (segundos + 59) / 60;
            if(minutos < 1) minutos = 1;
            float valor = minutos * 0.15;
            
            char andarNome[15];
            if(carros[i].andar == 0) sprintf(andarNome, "Térreo");
            else if(carros[i].andar == 1) sprintf(andarNome, "1º Andar");
            else sprintf(andarNome, "2º Andar");
            
            printf("[Rastreamento] Carro %d removido - %s vaga %d - %dmin - R$ %.2f\n", 
                   numeroCarro, andarNome, carros[i].vaga, minutos, valor);
            
            // Log para arquivo
            FILE *log = fopen("estacionamento_log.txt", "a");
            if(log) {
                time_t t = time(NULL);
                struct tm *tm_info = localtime(&t);
                char buffer[64];
                strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
                fprintf(log, "[%s] SAIDA - Carro %d - %s vaga %d - %dmin - R$ %.2f\n", 
                        buffer, numeroCarro, andarNome, carros[i].vaga, minutos, valor);
                fclose(log);
            }
            
            carros[i].ativo = false;
            pthread_mutex_unlock(&mutex_carros);
            return true;
        }
    }
    
    pthread_mutex_unlock(&mutex_carros);
    
    // ⚠️ ALERTA DE AUDITORIA - Carro saindo sem entrada registrada
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  ⚠️  ALERTA DE AUDITORIA - INCONSISTÊNCIA DETECTADA  ⚠️  ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  Carro #%d tentou SAIR sem registro de ENTRADA         ║\n", numeroCarro);
    printf("║  Possíveis causas:                                      ║\n");
    printf("║  • Falha no sensor de entrada                           ║\n");
    printf("║  • Entrada não registrada no sistema                    ║\n");
    printf("║  • Número de carro incorreto                            ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Log do alerta para arquivo
    FILE *log = fopen("estacionamento_log.txt", "a");
    if(log) {
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(log, "[%s] ⚠️ AUDITORIA - Carro %d saiu SEM ENTRADA REGISTRADA!\n", 
                buffer, numeroCarro);
        fclose(log);
    }
    
    return false;
}

/**
 * @brief Busca informações de um carro específico
 * @param numeroCarro Número do carro
 * @param andar Ponteiro para armazenar o andar (saída)
 * @param vaga Ponteiro para armazenar a vaga (saída)
 * @return true se encontrado, false caso contrário
 */
bool buscarCarro(int numeroCarro, int *andar, int *vaga) {
    pthread_mutex_lock(&mutex_carros);
    
    for(int i = 0; i < MAX_CARROS; i++) {
        if(carros[i].ativo && carros[i].numero == numeroCarro) {
            *andar = carros[i].andar;
            *vaga = carros[i].vaga;
            pthread_mutex_unlock(&mutex_carros);
            return true;
        }
    }
    
    pthread_mutex_unlock(&mutex_carros);
    return false;
}

/**
 * @brief Exibe o log recente do estacionamento
 */
void visualizarLog() {
    system("clear");
    printf("\n╔════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                   📜 LOG DE EVENTOS DO ESTACIONAMENTO                     ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════╝\n\n");
    
    FILE *log = fopen("estacionamento_log.txt", "r");
    if(!log) {
        printf("  ℹ️  Nenhum log disponível ainda.\n");
        printf("     O arquivo será criado automaticamente com as operações.\n\n");
    } else {
        char linha[256];
        int count = 0;
        
        // Conta linhas para mostrar apenas as últimas 30
        while(fgets(linha, sizeof(linha), log)) count++;
        
        rewind(log);
        int skip = (count > 30) ? (count - 30) : 0;
        count = 0;
        
        while(fgets(linha, sizeof(linha), log)) {
            if(count >= skip) {
                printf("%s", linha);
            }
            count++;
        }
        
        fclose(log);
        printf("\n  💡 Mostrando últimas 30 entradas (total: %d eventos)\n", count);
    }
    
    printf("\n");
    printf("Pressione ENTER para voltar ao menu...\n");
    limparBuffer();
    getchar();
}

/**
 * @brief Lista todos os carros estacionados com tempo e valor a pagar
 */
void listarTodosCarros() {
    pthread_mutex_lock(&mutex_carros);
    
    system("clear");
    printf("\n╔════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                  📋 CARROS ESTACIONADOS NO MOMENTO                        ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════╝\n\n");
    
    printf("┌────────┬──────────┬──────┬─────────────────┬──────────────────┐\n");
    printf("│ Carro  │  Andar   │ Vaga │ Tempo Estac.    │  Valor a Pagar   │\n");
    printf("├────────┼──────────┼──────┼─────────────────┼──────────────────┤\n");
    
    int totalCarros = 0;
    float totalArrecadado = 0.0;
    time_t agora = time(NULL);
    
    for(int i = 0; i < MAX_CARROS; i++) {
        if(carros[i].ativo) {
            char andarNome[15];
            if(carros[i].andar == 0) sprintf(andarNome, "Térreo");
            else if(carros[i].andar == 1) sprintf(andarNome, "1º Andar");
            else sprintf(andarNome, "2º Andar");
            
            // Calcula tempo total em minutos (arredonda para cima)
            int segundos = (int)difftime(agora, carros[i].timestamp);
            int minutosTotais = (segundos + 59) / 60; // Arredonda para cima: qualquer fração = 1 minuto
            int horas = minutosTotais / 60;
            int minutos = minutosTotais % 60;
            
            // Calcula valor a pagar (R$ 0,15 por minuto, mínimo R$ 0,15)
            if(minutosTotais < 1) minutosTotais = 1; // Mínimo de 1 minuto
            float valorAPagar = minutosTotais * 0.15;
            totalArrecadado += valorAPagar;
            
            printf("│   %2d   │ %-8s │  %2d  │  %2dh %2dmin      │   R$ %7.2f    │\n", 
                   carros[i].numero, andarNome, carros[i].vaga, horas, minutos, valorAPagar);
            totalCarros++;
        }
    }
    
    if(totalCarros == 0) {
        printf("│                    Nenhum carro estacionado                           │\n");
    }
    
    printf("└────────┴──────────┴──────┴─────────────────┴──────────────────┘\n");
    printf("\n");
    printf("  📊 Estatísticas:\n");
    printf("     • Total de carros: %d / %d\n", totalCarros, MAX_CARROS);
    printf("     • Arrecadação prevista: R$ %.2f\n", totalArrecadado);
    printf("     • Vagas livres: %d\n", MAX_CARROS - totalCarros);
    printf("\n");
    printf("  💡 Nota: Valores arredondados para cima (mínimo R$ 0,15)\n");
    printf("           Qualquer fração de minuto = 1 minuto completo\n");
    printf("\n");
    
    pthread_mutex_unlock(&mutex_carros);
    
    printf("Pressione ENTER para voltar ao menu...\n");
    limparBuffer();
    getchar();
}

void menu(pthread_t fRecebeTerreo, pthread_t fRecebePrimeiroAndar, pthread_t fRecebeSegundoAndar){

    bool pausarAtualizacao = false;

    while(1){
        if(!pausarAtualizacao){
            system("clear");
        }
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
        float y = (andar1[16])*0.15;
        float w = (andar2[16])*0.15;
        float z = (terreo[16])*0.15; 
        
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
            removerCarro(andar1[15]);  // Remove do rastreamento
            delay(1500);
        }
        if(andar2[14]==1){
            printf("\n              ------------------------------------\n");
            printf("             | Carro %d saiu da vaga B%d pagou %.2f |\n", andar2[15], andar2[17], w);
            printf("              ------------------------------------\n");
            removerCarro(andar2[15]);  // Remove do rastreamento
            delay(1500);
        }   
        if(terreo[14]==1){
            printf("\n              ------------------------------------\n");
            printf("             | Carro %d saiu da vaga T%d pagou %.2f |\n", terreo[15], terreo[17], z);
            printf("              ------------------------------------\n");
            removerCarro(terreo[15]);  // Remove do rastreamento
            delay(1500);
        }
        // Controle de entradas - previne duplicatas
        static int ultimoCarroTerreo = 0, ultimoCarroA1 = 0, ultimoCarroA2 = 0;
        
        if(andar1[11]==1 && andar1[12] != ultimoCarroA1){
            printf("\n                       ---------------------------\n");
            printf("                      | Carro %d entrou na vaga A%d |\n", andar1[12], andar1[13]);
            printf("                       ---------------------------\n");
            adicionarCarro(andar1[12], 1, andar1[13]);  // Registra no rastreamento
            ultimoCarroA1 = andar1[12];
            delay(1500);
        } else if(andar1[11] == 0) {
            ultimoCarroA1 = 0;  // Reseta quando flag desativa
        }
        
        if(andar2[11]==1 && andar2[12] != ultimoCarroA2){
            printf("\n                       ---------------------------\n");
            printf("                      | Carro %d entrou na vaga B%d |\n", andar2[12], andar2[13]);
            printf("                       ---------------------------\n");
            adicionarCarro(andar2[12], 2, andar2[13]);  // Registra no rastreamento
            ultimoCarroA2 = andar2[12];
            delay(1500);
        } else if(andar2[11] == 0) {
            ultimoCarroA2 = 0;  // Reseta quando flag desativa
        }
        
        if(terreo[11]==1 && terreo[12] != ultimoCarroTerreo){
            printf("\n                       ---------------------------\n");
            printf("                      | Carro %d entrou na vaga T%d |\n", terreo[12], terreo[13]);
            printf("                       ---------------------------\n");
            adicionarCarro(terreo[12], 0, terreo[13]);  // Registra no rastreamento
            ultimoCarroTerreo = terreo[12];
            delay(1500);
        } else if(terreo[11] == 0) {
            ultimoCarroTerreo = 0;  // Reseta quando flag desativa
        }

        printf("\n  Opções:\n");
        printf("  1 - Abrir estacionamento\n");
        printf("  2 - Fechar estacionamento\n");
        printf("  3 - Ativar 1 andar\n");
        printf("  4 - Desativar 1 andar\n");
        printf("  5 - Ativar 2 andar\n");
        printf("  6 - Desativar 2 andar\n");
        printf("  7 - 📋 Listar todos os carros\n");
        printf("  8 - 📜 Visualizar log de eventos\n");
        printf("  q - Encerrar estacionamento\n\n");      
        
        // Fechamento automático quando lotado (apenas se não estiver em modo manual)
        if((terreo[18] == 8 && r==0 && manual == 0 &&(andar1[18]== 8 || andar1[20] == 1) && (andar2[18] == 8 || andar2[20] == 1))){
            enviar[1] = 1;
            r = 1;
            registrarEvento("🔴 ESTACIONAMENTO FECHADO automaticamente (lotado)");
        } 
        // Reabertura automática (apenas se não estiver em modo manual de fechamento)
        else if((terreo[18] < 8 || andar1[20] == 0 || andar2[20] == 0) && r == 1 && manual == 0){
            enviar[1] = 0;          
            r = 0;
            registrarEvento("🟢 ESTACIONAMENTO ABERTO automaticamente (vagas disponíveis)");
        }
        
        if(kbhit()){
            char opcao = toupper(getchar());  // Converte para maiúscula
            pausarAtualizacao = true;
            
            switch(opcao)
            {
            case '1':
                system("clear");
                enviar[1] = 0;
                r =0;
                manual=0;
                printf("\n╔════════════════════════════════════════╗\n");
                printf("║   >>> ESTACIONAMENTO ABERTO <<<       ║\n");
                printf("╚════════════════════════════════════════╝\n");
                registrarEvento("🟢 ESTACIONAMENTO ABERTO manualmente");
                printf("\nPressione ENTER para continuar...\n");
                limparBuffer();
                getchar();
                pausarAtualizacao = false;
                break;
            case '2':
                system("clear");
                enviar[1] = 1;
                r = 1;
                manual = 1;
                printf("\n╔════════════════════════════════════════╗\n");
                printf("║   >>> ESTACIONAMENTO FECHADO <<<      ║\n");
                printf("╚════════════════════════════════════════╝\n");
                registrarEvento("🔴 ESTACIONAMENTO FECHADO manualmente");
                printf("\nPressione ENTER para continuar...\n");
                limparBuffer();
                getchar();
                pausarAtualizacao = false;
                break;
            case '3':
                system("clear");
                enviar[2] = 0;
                printf("\n╔════════════════════════════════════════╗\n");
                printf("║   >>> 1º ANDAR ATIVADO <<<            ║\n");
                printf("╚════════════════════════════════════════╝\n");
                registrarEvento("🟢 1º ANDAR ATIVADO");
                printf("\nPressione ENTER para continuar...\n");
                limparBuffer();
                getchar();
                pausarAtualizacao = false;
                break;
            case '4':
                system("clear");
                enviar[2] = 1;
                printf("\n╔════════════════════════════════════════╗\n");
                printf("║   >>> 1º ANDAR DESATIVADO <<<         ║\n");
                printf("╚════════════════════════════════════════╝\n");
                registrarEvento("🔴 1º ANDAR DESATIVADO");
                printf("\nPressione ENTER para continuar...\n");
                limparBuffer();
                getchar();
                pausarAtualizacao = false;
                break;
            case'5':
                system("clear");
                enviar[3] = 0;
                printf("\n╔════════════════════════════════════════╗\n");
                printf("║   >>> 2º ANDAR ATIVADO <<<            ║\n");
                printf("╚════════════════════════════════════════╝\n");
                registrarEvento("🟢 2º ANDAR ATIVADO");
                printf("\nPressione ENTER para continuar...\n");
                limparBuffer();
                getchar();
                pausarAtualizacao = false;
                break;
            case'6':
                system("clear");
                enviar[3] = 1;
                printf("\n╔════════════════════════════════════════╗\n");
                printf("║   >>> 2º ANDAR DESATIVADO <<<         ║\n");
                printf("╚════════════════════════════════════════╝\n");
                registrarEvento("🔴 2º ANDAR DESATIVADO");
                printf("\nPressione ENTER para continuar...\n");
                limparBuffer();
                getchar();
                pausarAtualizacao = false;
                break;
            case '7':
                listarTodosCarros();  // Lista todos os carros estacionados
                pausarAtualizacao = false;
                break;
            case '8':
                visualizarLog();  // Mostra log de eventos
                pausarAtualizacao = false;
                break;
            case 'Q':  // Aceita 'q' ou 'Q' (convertido por toupper)
                system("clear");
                printf("\n╔════════════════════════════════════════╗\n");
                printf("║   >>> ENCERRANDO ESTACIONAMENTO <<<   ║\n");
                printf("╚════════════════════════════════════════╝\n");
                delay(1000);
                pthread_cancel(fRecebeTerreo);
                pthread_cancel(fRecebePrimeiroAndar);
                pthread_cancel(fRecebeSegundoAndar);
                exit(0);
            case '\n':
            case '\r':
                // Ignora Enter e caracteres de controle
                pausarAtualizacao = false;
                break;
            default:
                pausarAtualizacao = false;
                break;
            }
            
        }
        
        if(!pausarAtualizacao){
            printf("\n");
            delay(1000);
        }
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
    
    while(1){
        recv(client_sock, andar1, tamVetorReceber * sizeof(int), 0);
        send (client_sock, enviar, tamVetorEnviar *sizeof(int) , 0);
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
    
    while(1){
        recv(client_sock, andar2, tamVetorReceber * sizeof(int), 0);
        send (client_sock, enviar, tamVetorEnviar *sizeof(int) , 0);
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
    
    while(1){
        recv(client_sock, terreo, tamVetorReceber * sizeof(int), 0);
        send (client_sock, enviar, tamVetorEnviar *sizeof(int) , 0);
        enviar[0] = terreo[12];
        delay(1000);
    }
    close(client_sock);
    printf("Client Disconnected\n");
}

int mainC(){
    //mainC
    
    // Inicializa o sistema de rastreamento de carros
    inicializarRastreamentoCarros();
    
    pthread_t fMenu,fRecebePrimeiroAndar, fRecebeSegundoAndar, fRecebeTerreo;
    //pthread_t fPassaCarro;

    
    pthread_create(&fRecebePrimeiroAndar, NULL, recebePrimeiroAndar, NULL);
    pthread_create(&fRecebeSegundoAndar, NULL, recebeSegundoAndar, NULL);
    pthread_create(&fRecebeTerreo, NULL, recebeTerreo, NULL);
    //pthread_create(&fPassaCarro, NULL, passaCarro, NULL);
    
    menu(fRecebeTerreo, fRecebePrimeiroAndar, fRecebeSegundoAndar);
    
    pthread_join(fRecebePrimeiroAndar, NULL);
    pthread_join(fRecebeSegundoAndar, NULL);
    pthread_join(fRecebeTerreo, NULL);
    //pthread_join(fPassaCarro, NULL);

    return 0;
}