#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inc/json_utils.h"

int main() {
    printf("=== TESTE DE IMPLEMENTAÇÃO JSON ===\n\n");
    
    // Teste 1: Evento de Entrada OK
    printf("1. Teste - Evento de Entrada OK:\n");
    entrada_ok_t entrada;
    strcpy(entrada.tipo, "entrada_ok");
    strcpy(entrada.placa, "ABC1D23");
    entrada.conf = 86;
    get_current_timestamp(entrada.ts, sizeof(entrada.ts));
    entrada.andar = 1;
    
    char json_buffer[512];
    serialize_entrada_ok(&entrada, json_buffer, sizeof(json_buffer));
    printf("JSON gerado:\n%s\n\n", json_buffer);
    
    // Teste 2: Evento de Saída OK
    printf("2. Teste - Evento de Saída OK:\n");
    saida_ok_t saida;
    strcpy(saida.tipo, "saida_ok");
    strcpy(saida.placa, "ABC1D23");
    saida.conf = 83;
    get_current_timestamp(saida.ts, sizeof(saida.ts));
    saida.andar = 1;
    
    serialize_saida_ok(&saida, json_buffer, sizeof(json_buffer));
    printf("JSON gerado:\n%s\n\n", saida.ts);
    
    // Teste 3: Status de Vagas
    printf("3. Teste - Status de Vagas:\n");
    vaga_status_t status;
    strcpy(status.tipo, "vaga_status");
    status.livres_a1 = 5;
    status.livres_a2 = 7;
    status.livres_total = 12;
    status.flags.lotado = 0;
    status.flags.bloq2 = 0;
    
    serialize_vaga_status(&status, json_buffer, sizeof(json_buffer));
    printf("JSON gerado:\n%s\n\n", json_buffer);
    
    // Teste 4: Evento de Passagem
    printf("4. Teste - Evento de Passagem:\n");
    passagem_t passagem;
    strcpy(passagem.tipo, "passagem");
    passagem.andar_origem = 1;
    passagem.andar_destino = 2;
    get_current_timestamp(passagem.ts, sizeof(passagem.ts));
    
    serialize_passagem(&passagem, json_buffer, sizeof(json_buffer));
    printf("JSON gerado:\n%s\n\n", json_buffer);
    
    // Teste 5: Evento de Alerta
    printf("5. Teste - Evento de Alerta:\n");
    alerta_t alerta;
    strcpy(alerta.tipo, "alerta");
    strcpy(alerta.mensagem, "Carro sem correspondência de entrada");
    strcpy(alerta.placa, "XYZ9876");
    get_current_timestamp(alerta.ts, sizeof(alerta.ts));
    
    serialize_alerta(&alerta, json_buffer, sizeof(json_buffer));
    printf("JSON gerado:\n%s\n\n", json_buffer);
    
    // Teste 6: Deserialização
    printf("6. Teste - Deserialização:\n");
    entrada_ok_t entrada_deserializada;
    const char* json_teste = "{\n  \"tipo\": \"entrada_ok\",\n  \"placa\": \"TEST123\",\n  \"conf\": 95,\n  \"ts\": \"2025-01-15T10:30:45Z\",\n  \"andar\": 2\n}";
    
    if (deserialize_entrada_ok(json_teste, &entrada_deserializada) == 0) {
        printf("Deserialização bem-sucedida:\n");
        printf("Tipo: %s\n", entrada_deserializada.tipo);
        printf("Placa: %s\n", entrada_deserializada.placa);
        printf("Confiança: %d\n", entrada_deserializada.conf);
        printf("Timestamp: %s\n", entrada_deserializada.ts);
        printf("Andar: %d\n", entrada_deserializada.andar);
    } else {
        printf("Erro na deserialização\n");
    }
    
    printf("\n=== TESTE CONCLUÍDO ===\n");
    return 0;
}
