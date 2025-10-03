#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Cliente Modbus para Sistema de Estacionamento
Implementação equivalente às funcionalidades das imagens Python
"""

import struct
import serial
import time
import threading
from typing import Optional, Union

class ModbusClient:
    def __init__(self, port: str = "/dev/ttyUSB0", baudrate: int = 115200):
        """
        Inicializa o cliente Modbus
        
        Args:
            port: Porta serial (ex: "/dev/ttyUSB0")
            baudrate: Taxa de transmissão (padrão: 115200)
        """
        self.port = port
        self.baudrate = baudrate
        self.uart = None
        self.is_connected = False
        
        # Endereços dos dispositivos
        self.CAMERA_ENTRADA_ADDR = 0x11
        self.CAMERA_SAIDA_ADDR = 0x12
        self.PLACAR_ADDR = 0x20
        
        # Códigos de função Modbus
        self.FUNC_READ_HOLDING_REGS = 0x03
        self.FUNC_WRITE_SINGLE_REG = 0x06
        self.FUNC_WRITE_MULTIPLE_REGS = 0x10
        
        # Matrícula do usuário (202017700)
        self.MATRICULA = "202017700"
        self.MATRICULA_BYTES = self._convert_matricula_to_little_endian()
        
        # Lock para thread safety
        self.lock = threading.Lock()
    
    def _convert_matricula_to_little_endian(self) -> bytes:
        """
        Converte a matrícula 202017700 para little-endian (4 bytes)
        
        Returns:
            bytes: Matrícula em formato little-endian
        """
        matricula_int = int(self.MATRICULA)
        # '<L' = little-endian unsigned long (4 bytes)
        return struct.pack('<L', matricula_int)
    
    def connect(self) -> bool:
        """
        Conecta ao dispositivo serial
        
        Returns:
            bool: True se conectou com sucesso
        """
        try:
            self.uart = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=1.0
            )
            self.is_connected = True
            print(f"Conectado ao dispositivo Modbus em {self.port}")
            return True
        except Exception as e:
            print(f"ERRO: Falha ao conectar Modbus: {e}")
            self.is_connected = False
            return False
    
    def disconnect(self):
        """Desconecta do dispositivo serial"""
        if self.uart and self.uart.is_open:
            self.uart.close()
        self.is_connected = False
        print("Desconectado do Modbus")
    
    def _limpar_buffer_serial(self):
        """Limpa o buffer serial"""
        if self.uart and self.uart.is_open:
            self.uart.reset_input_buffer()
            self.uart.reset_output_buffer()
    
    def _build_pdu(self, slave_addr: int, func_code: int, data: bytes) -> bytes:
        """
        Constrói o PDU (Protocol Data Unit) Modbus
        
        Args:
            slave_addr: Endereço do escravo
            func_code: Código da função
            data: Dados a serem enviados
            
        Returns:
            bytes: PDU completo com CRC
        """
        # Construir PDU core (equivalente à imagem)
        pdu_core = bytes([slave_addr, func_code]) + data + self.MATRICULA_BYTES
        
        # Calcular CRC (simplificado para exemplo)
        crc = self._calcula_crc(pdu_core)
        
        # Separar CRC em LSB e MSB (little-endian)
        crc_lsb = crc & 0xFF
        crc_msb = (crc >> 8) & 0xFF
        
        # Montar PDU completo
        full_pdu = pdu_core + bytes([crc_lsb, crc_msb])
        
        return full_pdu
    
    def _calcula_crc(self, data: bytes) -> int:
        """
        Calcula CRC16 Modbus (simplificado)
        
        Args:
            data: Dados para calcular CRC
            
        Returns:
            int: CRC calculado
        """
        # Implementação simplificada do CRC16 Modbus
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc >>= 1
        return crc
    
    def escrever_registrador(self, address: int, function_code: int, start_add: int, 
                           num_reg: int, byte_count: int, dados: bytes) -> Optional[bytes]:
        """
        Escreve dados em registradores Modbus (equivalente à imagem)
        
        Args:
            address: Endereço do escravo
            function_code: Código da função Modbus
            start_add: Endereço inicial do registrador
            num_reg: Número de registradores
            byte_count: Número de bytes
            dados: Dados a serem escritos
            
        Returns:
            Optional[bytes]: PDU da requisição ou None se erro
        """
        if not self.is_connected or not self.uart:
            print("ERRO: Conexão Modbus não estabelecida. Não é possível escrever.")
            return None
        
        # Preparar dados do cabeçalho (little-endian)
        header_data = struct.pack('<HHB', start_add, num_reg, byte_count)
        
        # Montar dados do PDU
        pdu_data = header_data + dados
        
        # Construir PDU completo
        req_pdu = self._build_pdu(address, function_code, pdu_data)
        
        try:
            with self.lock:
                self._limpar_buffer_serial()
                self.uart.write(req_pdu)
                print(f"Modbus 0x{function_code:02X} enviado para o slave {address}. Pacote: {req_pdu.hex()}")
                return req_pdu
        except Exception as e:
            print(f"ERRO DE COMUNICAÇÃO ao escrever: {e}")
            return None
    
    def trigger_cam_um(self) -> bool:
        """
        Dispara captura na câmera de entrada (equivalente à imagem)
        
        Returns:
            bool: True se enviado com sucesso
        """
        # Empacotar valor 1 em little-endian (equivalente à imagem)
        data_to_write = struct.pack("<H", 1)  # Little-endian unsigned short
        
        # Enviar comando de trigger
        req_pdu = self.escrever_registrador(
            address=self.CAMERA_ENTRADA_ADDR,
            function_code=self.FUNC_WRITE_MULTIPLE_REGS,
            start_add=1,
            num_reg=1,
            byte_count=2,
            dados=data_to_write
        )
        
        if req_pdu:
            print(f">> Trigger enviado: {req_pdu.hex()}")
            return True
        return False
    
    def trigger_cam_saida(self) -> bool:
        """
        Dispara captura na câmera de saída
        
        Returns:
            bool: True se enviado com sucesso
        """
        data_to_write = struct.pack("<H", 1)  # Little-endian unsigned short
        
        req_pdu = self.escrever_registrador(
            address=self.CAMERA_SAIDA_ADDR,
            function_code=self.FUNC_WRITE_MULTIPLE_REGS,
            start_add=1,
            num_reg=1,
            byte_count=2,
            dados=data_to_write
        )
        
        if req_pdu:
            print(f">> Trigger câmera saída enviado: {req_pdu.hex()}")
            return True
        return False
    
    def enviar_matricula_completa(self, slave_addr: int) -> bool:
        """
        Envia a matrícula completa (202017700) em little-endian
        
        Args:
            slave_addr: Endereço do escravo
            
        Returns:
            bool: True se enviado com sucesso
        """
        # Enviar matrícula em 2 registradores (4 bytes)
        req_pdu = self.escrever_registrador(
            address=slave_addr,
            function_code=self.FUNC_WRITE_MULTIPLE_REGS,
            start_add=15,  # Registrador 15 para matrícula
            num_reg=2,     # 2 registradores para 4 bytes
            byte_count=4,  # 4 bytes da matrícula
            dados=self.MATRICULA_BYTES
        )
        
        if req_pdu:
            print(f">> Matrícula {self.MATRICULA} enviada para slave {slave_addr:02X}: {req_pdu.hex()}")
            return True
        return False
    
    def enviar_ultimos_4_digitos(self, slave_addr: int) -> bool:
        """
        Envia os últimos 4 dígitos da matrícula (7700) em little-endian
        
        Args:
            slave_addr: Endereço do escravo
            
        Returns:
            bool: True se enviado com sucesso
        """
        # Extrair últimos 4 dígitos
        ultimos_4 = self.MATRICULA[-4:]  # "7700"
        ultimos_4_int = int(ultimos_4)
        
        # Converter para little-endian (2 bytes)
        data_to_write = struct.pack("<H", ultimos_4_int)
        
        req_pdu = self.escrever_registrador(
            address=slave_addr,
            function_code=self.FUNC_WRITE_SINGLE_REG,
            start_add=15,  # Registrador 15 para matrícula
            num_reg=1,     # 1 registrador para 2 bytes
            byte_count=2,  # 2 bytes
            dados=data_to_write
        )
        
        if req_pdu:
            print(f">> Últimos 4 dígitos ({ultimos_4}) enviados para slave {slave_addr:02X}: {req_pdu.hex()}")
            return True
        return False
    
    def ler_registradores(self, address: int, start_add: int, num_reg: int) -> Optional[bytes]:
        """
        Lê registradores Modbus
        
        Args:
            address: Endereço do escravo
            start_add: Endereço inicial
            num_reg: Número de registradores
            
        Returns:
            Optional[bytes]: Dados lidos ou None se erro
        """
        if not self.is_connected or not self.uart:
            print("ERRO: Conexão Modbus não estabelecida.")
            return None
        
        # Preparar dados para leitura
        data = struct.pack('<HH', start_add, num_reg)
        req_pdu = self._build_pdu(address, self.FUNC_READ_HOLDING_REGS, data)
        
        try:
            with self.lock:
                self._limpar_buffer_serial()
                self.uart.write(req_pdu)
                print(f"Modbus 0x{self.FUNC_READ_HOLDING_REGS:02X} enviado para o slave {address}, Pacote: {req_pdu.hex()}")
                
                # Aguardar resposta (simplificado)
                time.sleep(0.1)
                if self.uart.in_waiting > 0:
                    response = self.uart.read(self.uart.in_waiting)
                    print(f"Resposta recebida: {response.hex()}")
                    return response
                return None
        except Exception as e:
            print(f"ERRO DE COMUNICAÇÃO ao ler: {e}")
            return None
    
    def atualizar_placar(self, vagas_terreo: int, vagas_andar1: int, vagas_andar2: int) -> bool:
        """
        Atualiza dados do placar
        
        Args:
            vagas_terreo: Vagas disponíveis no térreo
            vagas_andar1: Vagas disponíveis no 1º andar
            vagas_andar2: Vagas disponíveis no 2º andar
            
        Returns:
            bool: True se atualizado com sucesso
        """
        # Preparar dados do placar (12 registradores)
        placar_data = struct.pack('<HHHHHHHHHHHH',
            vagas_terreo,  # Registrador 0
            vagas_andar1,  # Registrador 1
            vagas_andar2,  # Registrador 2
            0, 0, 0, 0, 0, 0, 0, 0, 0  # Outros registradores
        )
        
        req_pdu = self.escrever_registrador(
            address=self.PLACAR_ADDR,
            function_code=self.FUNC_WRITE_MULTIPLE_REGS,
            start_add=0,
            num_reg=12,
            byte_count=24,  # 12 registradores * 2 bytes
            dados=placar_data
        )
        
        if req_pdu:
            print(f">> Placar atualizado: {req_pdu.hex()}")
            return True
        return False


def main():
    """Função principal para teste"""
    print("=== CLIENTE MODBUS SISTEMA DE ESTACIONAMENTO ===")
    print(f"Matrícula: 202017700")
    print(f"Matrícula em little-endian: {struct.pack('<L', 202017700).hex()}")
    print(f"Últimos 4 dígitos: 7700")
    print(f"Últimos 4 dígitos em little-endian: {struct.pack('<H', 7700).hex()}")
    
    # Criar cliente
    client = ModbusClient()
    
    # Conectar
    if not client.connect():
        print("Falha ao conectar. Encerrando.")
        return
    
    try:
        print("\n=== TESTANDO FUNCIONALIDADES ===")
        
        # Teste 1: Trigger câmera entrada
        print("\n1. Testando trigger câmera entrada...")
        client.trigger_cam_um()
        
        # Teste 2: Trigger câmera saída
        print("\n2. Testando trigger câmera saída...")
        client.trigger_cam_saida()
        
        # Teste 3: Enviar matrícula completa
        print("\n3. Enviando matrícula completa...")
        client.enviar_matricula_completa(client.CAMERA_ENTRADA_ADDR)
        
        # Teste 4: Enviar últimos 4 dígitos
        print("\n4. Enviando últimos 4 dígitos...")
        client.enviar_ultimos_4_digitos(client.PLACAR_ADDR)
        
        # Teste 5: Atualizar placar
        print("\n5. Atualizando placar...")
        client.atualizar_placar(4, 8, 8)  # 4 térreo, 8 andar1, 8 andar2
        
        # Teste 6: Ler registradores
        print("\n6. Lendo registradores...")
        client.ler_registradores(client.CAMERA_ENTRADA_ADDR, 0, 8)
        
        print("\n=== TESTES CONCLUÍDOS ===")
        
    except KeyboardInterrupt:
        print("\nInterrompido pelo usuário")
    finally:
        client.disconnect()


if __name__ == "__main__":
    main()
