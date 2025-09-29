#ifndef MODBUS_UTILS_H
#define MODBUS_UTILS_H

#include <modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// Endereços MODBUS dos dispositivos
#define CAMERA_ENTRADA_ADDR 0x11
#define CAMERA_SAIDA_ADDR 0x12
#define PLACAR_ADDR 0x20

// Offsets dos registros das câmeras LPR
#define STATUS_REG 0
#define TRIGGER_REG 1
#define PLACA_REG_START 2
#define PLACA_REG_COUNT 4
#define CONFIANCA_REG 6
#define ERRO_REG 7

// Offsets dos registros do placar
#define VAGAS_TERREO_PCD 0
#define VAGAS_TERREO_IDOSO 1
#define VAGAS_TERREO_COMUM 2
#define VAGAS_ANDAR1_PCD 3
#define VAGAS_ANDAR1_IDOSO 4
#define VAGAS_ANDAR1_COMUM 5
#define VAGAS_ANDAR2_PCD 6
#define VAGAS_ANDAR2_IDOSO 7
#define VAGAS_ANDAR2_COMUM 8
#define VAGAS_TOTAL_PCD 9
#define VAGAS_TOTAL_IDOSO 10
#define VAGAS_TOTAL_COMUM 11
#define FLAGS_REG 12

// Status da câmera
#define CAMERA_PRONTA 0
#define CAMERA_PROCESSANDO 1
#define CAMERA_OK 2
#define CAMERA_ERRO 3

// Estrutura para dados da câmera LPR
typedef struct {
    char placa[9];  // 8 caracteres + null terminator
    int confianca;
    int status;
    int erro;
} lpr_data_t;

// Estrutura para dados do placar
typedef struct {
    int vagas_terreo_pcd;
    int vagas_terreo_idoso;
    int vagas_terreo_comum;
    int vagas_andar1_pcd;
    int vagas_andar1_idoso;
    int vagas_andar1_comum;
    int vagas_andar2_pcd;
    int vagas_andar2_idoso;
    int vagas_andar2_comum;
    int vagas_total_pcd;
    int vagas_total_idoso;
    int vagas_total_comum;
    int flags;  // bit0=lotado_geral, bit1=lotado_andar1, bit2=lotado_andar2
} placar_data_t;

// Funções MODBUS
modbus_t* init_modbus_connection(const char* device, int baudrate);
void close_modbus_connection(modbus_t* ctx);

// Funções das câmeras LPR
int trigger_camera_capture(modbus_t* ctx, int camera_addr);
int read_camera_status(modbus_t* ctx, int camera_addr);
int read_camera_data(modbus_t* ctx, int camera_addr, lpr_data_t* data);
int capture_license_plate(modbus_t* ctx, int camera_addr, lpr_data_t* data, int timeout_ms);

// Funções do placar
int update_placar_data(modbus_t* ctx, const placar_data_t* data);
int read_placar_data(modbus_t* ctx, placar_data_t* data);

// Funções utilitárias
void convert_placa_from_registers(uint16_t* registers, char* placa);
void convert_placa_to_registers(const char* placa, uint16_t* registers);
int modbus_retry_operation(modbus_t* ctx, int (*operation)(modbus_t*, void*), void* data, int max_retries);

#endif
