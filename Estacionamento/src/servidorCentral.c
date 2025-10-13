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
#include "../inc/modbus.h"

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
    int numero;           // Número do carro (ID sequencial ou ticket temporário)
    char placa[9];        // Placa do veículo (8 chars + \0) ou "TEMP####" para temporários
    int confianca;        // Confiança da leitura (0-100%), -1 se não aplicável
    int andar;            // 0=Térreo, 1=1ºAndar, 2=2ºAndar, -1=Vazio
    int vaga;             // Número da vaga (1-4 térreo, 1-8 andares)
    time_t timestamp;     // Hora de entrada
    bool ativo;           // true=estacionado, false=slot vazio
    bool ticket_temporario; // true se é ticket temporário (placa não lida)
    bool reconciliado;    // true se ticket foi reconciliado manualmente
} CarroEstacionado;

// Array global para rastrear todos os carros
CarroEstacionado carros[MAX_CARROS];
pthread_mutex_t mutex_carros = PTHREAD_MUTEX_INITIALIZER;

// ⚠️ MODBUS removido do Central - agora centralizado no Térreo conforme especificação
// O Central envia dados do placar via TCP/IP para o Térreo, que escreve no MODBUS

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
        strcpy(carros[i].placa, "");
        carros[i].confianca = -1;
        carros[i].andar = -1;
        carros[i].vaga = 0;
        carros[i].timestamp = 0;
        carros[i].ativo = false;
        carros[i].ticket_temporario = false;
        carros[i].reconciliado = false;
    }
    pthread_mutex_unlock(&mutex_carros);
    printf("[Sistema] Rastreamento de carros inicializado\n");
    registrarEvento("🚀 SISTEMA INICIADO - Rastreamento ativo com suporte LPR");
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
    
    // ✅ CORREÇÃO: Verifica se o carro já está ATIVO no sistema
    // Se encontrar registro antigo, remove automaticamente (limpeza)
    for(int i = 0; i < MAX_CARROS; i++) {
        if(carros[i].ativo && carros[i].numero == numeroCarro) {
            printf("[Rastreamento] ⚠️  Carro %d já registrado (andar %d vaga %d)\n", 
                   numeroCarro, carros[i].andar, carros[i].vaga);
            printf("[Rastreamento] 🔧 Removendo registro antigo e criando novo...\n");
            carros[i].ativo = false;  // Remove registro antigo
            break;  // Continua para adicionar novo registro
        }
    }
    
    // Busca um slot vazio
    for(int i = 0; i < MAX_CARROS; i++) {
        if(!carros[i].ativo) {
            carros[i].numero = numeroCarro;
            strcpy(carros[i].placa, "");  // Será preenchido pelo LPR se disponível
            carros[i].confianca = -1;
            carros[i].andar = andar;
            carros[i].vaga = vaga;
            carros[i].timestamp = time(NULL);
            carros[i].ativo = true;
            carros[i].ticket_temporario = false;
            carros[i].reconciliado = false;
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
 * @brief Adiciona um carro com placa LPR ao sistema
 * @param numeroCarro Número do carro
 * @param placa Placa lida pela câmera LPR
 * @param confianca Confiança da leitura (0-100%)
 * @param andar Andar onde está
 * @param vaga Número da vaga
 * @return true se adicionado com sucesso
 */
bool adicionarCarroComPlaca(int numeroCarro, const char *placa, int confianca, int andar, int vaga) {
    pthread_mutex_lock(&mutex_carros);
    
    // Limiar de confiança: 70% (conforme especificação)
    bool baixa_confianca = (confianca < 70);
    
    // Busca um slot vazio
    for(int i = 0; i < MAX_CARROS; i++) {
        if(!carros[i].ativo) {
            carros[i].numero = numeroCarro;
            
            if(baixa_confianca || strlen(placa) == 0) {
                // Ticket temporário para placas não lidas ou baixa confiança
                sprintf(carros[i].placa, "TEMP%04d", numeroCarro);
                carros[i].ticket_temporario = true;
                carros[i].reconciliado = false;
            } else {
                // Placa com confiança adequada
                strncpy(carros[i].placa, placa, 8);
                carros[i].placa[8] = '\0';
                carros[i].ticket_temporario = false;
                carros[i].reconciliado = true; // Já possui placa válida
            }
            
            carros[i].confianca = confianca;
            carros[i].andar = andar;
            carros[i].vaga = vaga;
            carros[i].timestamp = time(NULL);
            carros[i].ativo = true;
            pthread_mutex_unlock(&mutex_carros);
            
            char andarNome[15];
            if(andar == 0) sprintf(andarNome, "Térreo");
            else if(andar == 1) sprintf(andarNome, "1º Andar");
            else sprintf(andarNome, "2º Andar");
            
            if(carros[i].ticket_temporario) {
                printf("[Rastreamento] 🎫 Ticket temporário %s (ID %d) → %s vaga %d (confiança: %d%%)\n", 
                       carros[i].placa, numeroCarro, andarNome, vaga, confianca);
            } else {
                printf("[Rastreamento] 🚗 Placa %s (ID %d) → %s vaga %d (confiança: %d%%)\n", 
                       carros[i].placa, numeroCarro, andarNome, vaga, confianca);
            }
            
            // Log para arquivo
            FILE *log = fopen("estacionamento_log.txt", "a");
            if(log) {
                time_t t = time(NULL);
                struct tm *tm_info = localtime(&t);
                char buffer[64];
                strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
                if(carros[i].ticket_temporario) {
                    fprintf(log, "[%s] ENTRADA - Ticket %s → %s vaga %d (LPR conf=%d%% - BAIXA)\n", 
                            buffer, carros[i].placa, andarNome, vaga, confianca);
                } else {
                    fprintf(log, "[%s] ENTRADA - Placa %s → %s vaga %d (LPR conf=%d%%)\n", 
                            buffer, carros[i].placa, andarNome, vaga, confianca);
                }
                fclose(log);
            }
            
            return true;
        }
    }
    
    pthread_mutex_unlock(&mutex_carros);
    printf("[Rastreamento] ERRO: Capacidade máxima atingida!\n");
    return false;
}

/**
 * @brief Remove um carro do sistema de rastreamento com validação de auditoria
 * @param numeroCarro Número do carro a remover
 * @return true se removido com sucesso, false se não encontrado
 */
bool removerCarro(int numeroCarro) {
    pthread_mutex_lock(&mutex_carros);
    
    // ✅ CORREÇÃO: Remove APENAS o carro mais recente com esse número
    int indice_mais_recente = -1;
    time_t timestamp_mais_recente = 0;
    
    for(int i = 0; i < MAX_CARROS; i++) {
        if(carros[i].ativo && carros[i].numero == numeroCarro) {
            if(indice_mais_recente == -1 || carros[i].timestamp > timestamp_mais_recente) {
                indice_mais_recente = i;
                timestamp_mais_recente = carros[i].timestamp;
            }
        }
    }
    
    if(indice_mais_recente >= 0) {
        int i = indice_mais_recente;
        
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
 * @brief Reconcilia um ticket temporário com uma placa real
 * @param numeroCarro ID do carro/ticket
 * @param placaReal Placa real informada manualmente
 */
bool reconciliarTicket(int numeroCarro, const char *placaReal) {
    pthread_mutex_lock(&mutex_carros);
    
    for(int i = 0; i < MAX_CARROS; i++) {
        if(carros[i].ativo && carros[i].numero == numeroCarro && carros[i].ticket_temporario) {
            char ticketAntigo[9];
            strcpy(ticketAntigo, carros[i].placa);
            
            strncpy(carros[i].placa, placaReal, 8);
            carros[i].placa[8] = '\0';
            carros[i].ticket_temporario = false;
            carros[i].reconciliado = true;
            carros[i].confianca = 100; // Manualmente verificado
            
            pthread_mutex_unlock(&mutex_carros);
            
            printf("[Reconciliação] ✅ Ticket %s → Placa %s\n", ticketAntigo, placaReal);
            
            // Log para arquivo
            FILE *log = fopen("estacionamento_log.txt", "a");
            if(log) {
                time_t t = time(NULL);
                struct tm *tm_info = localtime(&t);
                char buffer[64];
                strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
                fprintf(log, "[%s] RECONCILIAÇÃO - Ticket %s → Placa %s (ID %d)\n", 
                        buffer, ticketAntigo, placaReal, numeroCarro);
                fclose(log);
            }
            
            return true;
        }
    }
    
    pthread_mutex_unlock(&mutex_carros);
    printf("[Reconciliação] ❌ Ticket ID %d não encontrado ou já reconciliado\n", numeroCarro);
    return false;
}

/**
 * @brief Lista todos os tickets temporários pendentes de reconciliação
 */
void listarTicketsTemporarios() {
    pthread_mutex_lock(&mutex_carros);
    
    system("clear");
    printf("\n╔════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║              🎫 TICKETS TEMPORÁRIOS PENDENTES DE RECONCILIAÇÃO            ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════╝\n\n");
    
    printf("┌────────┬──────────────┬──────────┬──────┬─────────────────┬──────────────┐\n");
    printf("│   ID   │    Ticket    │  Andar   │ Vaga │ Tempo Estac.    │  Confiança   │\n");
    printf("├────────┼──────────────┼──────────┼──────┼─────────────────┼──────────────┤\n");
    
    int totalTickets = 0;
    time_t agora = time(NULL);
    
    for(int i = 0; i < MAX_CARROS; i++) {
        if(carros[i].ativo && carros[i].ticket_temporario && !carros[i].reconciliado) {
            char andarNome[15];
            if(carros[i].andar == 0) sprintf(andarNome, "Térreo");
            else if(carros[i].andar == 1) sprintf(andarNome, "1º Andar");
            else sprintf(andarNome, "2º Andar");
            
            int segundos = (int)difftime(agora, carros[i].timestamp);
            int minutosTotais = (segundos + 59) / 60;
            int horas = minutosTotais / 60;
            int minutos = minutosTotais % 60;
            
            printf("│  %4d  │  %-10s  │ %-8s │  %2d  │  %2dh %2dmin      │    %3d%%     │\n", 
                   carros[i].numero, carros[i].placa, andarNome, carros[i].vaga, 
                   horas, minutos, carros[i].confianca);
            totalTickets++;
        }
    }
    
    if(totalTickets == 0) {
        printf("│           ✅ Nenhum ticket temporário pendente de reconciliação          │\n");
    }
    
    printf("└────────┴──────────────┴──────────┴──────┴─────────────────┴──────────────┘\n");
    printf("\n");
    printf("  📊 Estatísticas:\n");
    printf("     • Total de tickets pendentes: %d\n", totalTickets);
    printf("     • Limiar de confiança: 70%% (abaixo disso gera ticket temporário)\n");
    printf("\n");
    
    pthread_mutex_unlock(&mutex_carros);
    
    // Interface de reconciliação
    if(totalTickets > 0) {
        printf("  ┌────────────────────────────────────────────────────────────────┐\n");
        printf("  │  Deseja reconciliar algum ticket?                             │\n");
        printf("  │  Digite o ID do ticket (ou 0 para voltar):                    │\n");
        printf("  └────────────────────────────────────────────────────────────────┘\n");
        printf("  ID: ");
        
        int idTicket;
        scanf("%d", &idTicket);
        limparBuffer();
        
        if(idTicket > 0) {
            printf("\n  Digite a placa real (8 caracteres): ");
            char placaReal[9];
            fgets(placaReal, sizeof(placaReal), stdin);
            placaReal[strcspn(placaReal, "\n")] = '\0'; // Remove newline
            
            // Converte para maiúsculas
            for(int i = 0; placaReal[i]; i++) {
                placaReal[i] = toupper(placaReal[i]);
            }
            
            if(strlen(placaReal) >= 7 && strlen(placaReal) <= 8) {
                if(reconciliarTicket(idTicket, placaReal)) {
                    printf("\n  ✅ Reconciliação realizada com sucesso!\n");
                } else {
                    printf("\n  ❌ Erro na reconciliação. Verifique o ID do ticket.\n");
                }
            } else {
                printf("\n  ❌ Placa inválida. Deve ter 7-8 caracteres.\n");
            }
            
            printf("\n  Pressione ENTER para continuar...\n");
            getchar();
        }
    } else {
        printf("  Pressione ENTER para voltar ao menu...\n");
        getchar();
    }
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
    
    printf("┌─────┬──────────────┬───────────┬──────┬──────────────┬─────────────────┐\n");
    printf("│ ID  │ Placa/Ticket │   Andar   │ Vaga │ Tempo Estac. │  Valor a Pagar  │\n");
    printf("├─────┼──────────────┼───────────┼──────┼──────────────┼─────────────────┤\n");
    
    int totalCarros = 0;
    int totalTickets = 0;
    int totalComPlaca = 0;
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
            
            // Formata placa/ticket com indicador visual CLARO
            char identificador[15];
            if(carros[i].ticket_temporario) {
                // Ticket temporário (placa não lida ou baixa confiança < 70%)
                sprintf(identificador, "🎫 %s", carros[i].placa);
                totalTickets++;
            } else if(strlen(carros[i].placa) > 0 && strncmp(carros[i].placa, "TEMP", 4) != 0) {
                // Placa IDENTIFICADA pelo LPR (confiança >= 70%)
                sprintf(identificador, "🚗 %-8s", carros[i].placa);
                totalComPlaca++;
            } else {
                // ID anônimo (não deveria acontecer, mas como fallback)
                sprintf(identificador, "ID-%04d", carros[i].numero);
            }
            
            printf("│%4d │ %-12s │ %-9s │  %2d  │ %2dh %2dmin     │   R$ %7.2f   │\n", 
                   carros[i].numero, identificador, andarNome, carros[i].vaga, horas, minutos, valorAPagar);
            totalCarros++;
        }
    }
    
    if(totalCarros == 0) {
        printf("│                       Nenhum carro estacionado                            │\n");
    }
    
    printf("└─────┴──────────────┴───────────┴──────┴──────────────┴─────────────────┘\n");
    printf("\n");
    
    // ✅ Calcula vagas totais considerando andares bloqueados
    int vagasTotais = MAX_CARROS;  // Inicia com 20 vagas
    int vagasBloqueadas = 0;
    
    // Se Térreo está fechado manualmente: remove 4 vagas do total
    if(enviar[1] == 1) {
        vagasBloqueadas += 4;
    }
    
    // Se 1º Andar está fechado manualmente: remove 8 vagas do total
    if(enviar[2] == 1) {
        vagasBloqueadas += 8;
    }
    
    // Se 2º Andar está fechado manualmente: remove 8 vagas do total
    if(enviar[3] == 1) {
        vagasBloqueadas += 8;
    }
    
    vagasTotais -= vagasBloqueadas;  // Total de vagas disponíveis após bloqueios
    int vagasLivres = vagasTotais - totalCarros;
    
    printf("  📊 Estatísticas:\n");
    printf("     • Total de carros: %d / %d", totalCarros, vagasTotais);
    
    // Mostra aviso se há andares bloqueados
    if(vagasBloqueadas > 0) {
        printf(" (%d vagas bloqueadas)", vagasBloqueadas);
    }
    printf("\n");
    
    printf("     • Com placa LPR: %d carros\n", totalComPlaca);
    printf("     • Tickets temporários: %d (necessitam reconciliação)\n", totalTickets);
    printf("     • Arrecadação prevista: R$ %.2f\n", totalArrecadado);
    printf("     • Vagas livres: %d", vagasLivres);
    
    // Detalhamento de vagas livres por andar
    if(vagasBloqueadas > 0) {
        printf(" (");
        bool primeiro = true;
        
        if(enviar[1] == 0) {  // Térreo ativo
            int vagasTerreo = terreo[0] + terreo[1] + terreo[2];
            printf("Térreo: %d", vagasTerreo);
            primeiro = false;
        }
        
        if(enviar[2] == 0) {  // 1º Andar ativo
            int vagas1Andar = andar1[0] + andar1[1] + andar1[2];
            if(!primeiro) printf(", ");
            printf("1º Andar: %d", vagas1Andar);
            primeiro = false;
        }
        
        if(enviar[3] == 0) {  // 2º Andar ativo
            int vagas2Andar = andar2[0] + andar2[1] + andar2[2];
            if(!primeiro) printf(", ");
            printf("2º Andar: %d", vagas2Andar);
        }
        
        printf(")");
    }
    printf("\n");
    
    // Mostra quais andares estão bloqueados
    if(vagasBloqueadas > 0) {
        printf("     • ⚠️  Andares bloqueados: ");
        bool primeiro = true;
        
        if(enviar[1] == 1) {
            printf("Térreo (4 vagas)");
            primeiro = false;
        }
        if(enviar[2] == 1) {
            if(!primeiro) printf(", ");
            printf("1º Andar (8 vagas)");
            primeiro = false;
        }
        if(enviar[3] == 1) {
            if(!primeiro) printf(", ");
            printf("2º Andar (8 vagas)");
        }
        printf("\n");
    }
    
    printf("\n");
    printf("  💡 Notas:\n");
    printf("     • Valores arredondados para cima (mínimo R$ 0,15)\n");
    printf("     • Qualquer fração de minuto = 1 minuto completo\n");
    printf("     • 🎫 = Ticket temporário (placa não lida ou confiança < 70%%)\n");
    printf("     • 🚗 = Placa identificada por LPR (confiança ≥ 70%%)\n");
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
        printf("  9 - 🎫 Reconciliar tickets temporários (LPR)\n");
        printf("  q - Encerrar estacionamento\n\n");      
        
        // ✅ CORREÇÃO: Fechamento automático quando lotado (20 carros no total)
        // Conforme especificação: 20 vagas totais (4 térreo + 8 andar1 + 8 andar2)
        int totalCarrosAtual = terreo[18] + andar1[18] + andar2[18];
        
        if(totalCarrosAtual >= 20 && r == 0 && manual == 0){
            enviar[1] = 1;
            r = 1;
            registrarEvento("🔴 ESTACIONAMENTO FECHADO automaticamente (lotado - 20 vagas ocupadas)");
            printf("\n⚠️  ESTACIONAMENTO LOTADO - Total: %d carros (T:%d A1:%d A2:%d)\n", 
                   totalCarrosAtual, terreo[18], andar1[18], andar2[18]);
        } 
        // ✅ CORREÇÃO: Reabertura automática quando há vagas disponíveis
        else if(totalCarrosAtual < 20 && r == 1 && manual == 0){
            enviar[1] = 0;          
            r = 0;
            registrarEvento("🟢 ESTACIONAMENTO ABERTO automaticamente (vagas disponíveis)");
            printf("\n✅ ESTACIONAMENTO REABERTO - Total: %d carros (T:%d A1:%d A2:%d)\n", 
                   totalCarrosAtual, terreo[18], andar1[18], andar2[18]);
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
            case '9':
                listarTicketsTemporarios();  // Lista e reconcilia tickets temporários
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
        
        // ✅ Detecta e registra passagem entre andares
        if(andar1[22] == 1){  // Flag de evento de passagem
            char mensagem[200];
            if(andar1[21] == 1){
                // Subindo: Térreo → 1º Andar
                sprintf(mensagem, "🚗↑ Veículo SUBINDO: Térreo → 1º Andar");
            } else if(andar1[21] == 2){
                // Descendo: 1º Andar → Térreo
                sprintf(mensagem, "🚗↓ Veículo DESCENDO: 1º Andar → Térreo");
            }
            registrarEvento(mensagem);
            printf("%s\n", mensagem);
        }
        
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
        
        // ✅ Detecta e registra passagem entre andares
        if(andar2[22] == 1){  // Flag de evento de passagem
            char mensagem[200];
            if(andar2[21] == 1){
                // Subindo: 1º Andar → 2º Andar
                sprintf(mensagem, "🚗↑ Veículo SUBINDO: 1º Andar → 2º Andar");
            } else if(andar2[21] == 2){
                // Descendo: 2º Andar → 1º Andar
                sprintf(mensagem, "🚗↓ Veículo DESCENDO: 2º Andar → 1º Andar");
            }
            registrarEvento(mensagem);
            printf("%s\n", mensagem);
        }
        
        delay(1000);
    }
    close(client_sock);
    printf("Client 2 Disconnected\n");
}

// ⚠️ Thread removida do Central - MODBUS agora centralizado no Térreo
// Conforme especificação (Seção 2.2): "publicar estatísticas no placar MODBUS"
// e "Orientação: centralizar a interface MODBUS no servidor distribuído do Andar Térreo"
//
// O Central agora COMANDA o Térreo via TCP/IP para escrever no placar

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
    
    // Array para enviar dados do placar MODBUS ao Térreo
    int dadosPlacar[14];
    
    while(1){
        recv(client_sock, terreo, tamVetorReceber * sizeof(int), 0);
        send (client_sock, enviar, tamVetorEnviar *sizeof(int) , 0);
        enviar[0] = terreo[12];
        
        // ✅ NOVO: Calcula e envia dados do placar MODBUS
        // Prepara dados de vagas livres por tipo e andar
        dadosPlacar[0] = terreo[0];   // Vagas livres Térreo PNE
        dadosPlacar[1] = terreo[1];   // Vagas livres Térreo Idoso
        dadosPlacar[2] = terreo[2];   // Vagas livres Térreo Comuns
        dadosPlacar[3] = andar1[0];   // Vagas livres 1º Andar PNE
        dadosPlacar[4] = andar1[1];   // Vagas livres 1º Andar Idoso
        dadosPlacar[5] = andar1[2];   // Vagas livres 1º Andar Comuns
        dadosPlacar[6] = andar2[0];   // Vagas livres 2º Andar PNE
        dadosPlacar[7] = andar2[1];   // Vagas livres 2º Andar Idoso
        dadosPlacar[8] = andar2[2];   // Vagas livres 2º Andar Comuns
        dadosPlacar[9] = terreo[18];  // Número de carros Térreo
        dadosPlacar[10] = andar1[18]; // Número de carros 1º Andar
        dadosPlacar[11] = andar2[18]; // Número de carros 2º Andar
        
        // ✅ CALCULA FLAGS (bit0, bit1, bit2) para luzes vermelhas do placar MODBUS
        int flags = 0;
        
        // MODO AUTOMÁTICO + MANUAL:
        // bit0 = Estacionamento lotado (20 vagas ocupadas) OU fechado manualmente
        int totalCarros = terreo[18] + andar1[18] + andar2[18];
        if(totalCarros >= 20 || enviar[1] == 1) {
            flags |= (1 << 0);  // Acende luz vermelha da ENTRADA
        }
        
        // bit1 = 1º Andar lotado (8 vagas ocupadas) OU bloqueado manualmente
        if(andar1[18] >= 8 || enviar[2] == 1) {
            flags |= (1 << 1);  // Acende luz vermelha do 1º ANDAR
        }
        
        // bit2 = 2º Andar lotado (8 vagas ocupadas) OU bloqueado manualmente
        if(andar2[18] >= 8 || enviar[3] == 1) {
            flags |= (1 << 2);  // Acende luz vermelha do 2º ANDAR
        }
        
        // DEBUG: Log de flags quando há mudança
        static int flags_anterior = -1;
        if(flags != flags_anterior) {
            printf("[PLACAR-MODBUS] Flags atualizadas: 0x%02X (bit0=%d entrada, bit1=%d 1ºAndar, bit2=%d 2ºAndar)\n", 
                   flags, (flags & 0x01) ? 1 : 0, (flags & 0x02) ? 1 : 0, (flags & 0x04) ? 1 : 0);
            printf("[PLACAR-MODBUS] Estado: enviar[1]=%d (fechado geral), enviar[2]=%d (1º bloqueado), enviar[3]=%d (2º bloqueado)\n",
                   enviar[1], enviar[2], enviar[3]);
            flags_anterior = flags;
        }
        
        dadosPlacar[12] = flags;  // Flags para o placar MODBUS
        dadosPlacar[13] = 1;      // Comando: atualizar placar
        
        // Envia dados do placar ao Térreo para ele escrever no MODBUS
        send(client_sock, dadosPlacar, 14 * sizeof(int), 0);
        
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
    // ✅ fPlacarModbus removida - thread agora no Térreo

    
    pthread_create(&fRecebePrimeiroAndar, NULL, recebePrimeiroAndar, NULL);
    pthread_create(&fRecebeSegundoAndar, NULL, recebeSegundoAndar, NULL);
    pthread_create(&fRecebeTerreo, NULL, recebeTerreo, NULL);
    // ✅ Thread de MODBUS removida - agora no Térreo conforme especificação
    //pthread_create(&fPassaCarro, NULL, passaCarro, NULL);
    
    menu(fRecebeTerreo, fRecebePrimeiroAndar, fRecebeSegundoAndar);
    
    // ✅ MODBUS cleanup removido - agora gerenciado pelo Térreo
    
    pthread_join(fRecebePrimeiroAndar, NULL);
    pthread_join(fRecebeSegundoAndar, NULL);
    pthread_join(fRecebeTerreo, NULL);
    // ✅ Thread de MODBUS removida
    //pthread_join(fPassaCarro, NULL);

    return 0;
}