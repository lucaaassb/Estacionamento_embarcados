"""
Módulo de Cliente MODBUS RTU
Gerencia comunicação com dispositivos MODBUS (câmeras LPR e placar)
"""

import serial
import struct
import time
import logging
from typing import Optional, List, Tuple
from enum import IntEnum

logger = logging.getLogger(__name__)

class ModbusFunction(IntEnum):
    """Códigos de função MODBUS"""
    READ_HOLDING_REGISTERS = 0x03
    WRITE_MULTIPLE_REGISTERS = 0x10

class ModbusException(Exception):
    """Exceção para erros MODBUS"""
    pass

class ModbusClient:
    """Cliente MODBUS RTU para comunicação serial"""
    
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 0.5):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial = None
        self.matricula = "1606"  # 4 últimos dígitos da matrícula
    
    def conectar(self):
        """Abre conexão serial"""
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self.timeout
            )
            logger.info(f"Conexão MODBUS aberta em {self.port}")
        except Exception as e:
            logger.error(f"Erro ao abrir porta serial: {e}")
            raise
    
    def desconectar(self):
        """Fecha conexão serial"""
        if self.serial and self.serial.is_open:
            self.serial.close()
            logger.info("Conexão MODBUS fechada")
    
    def set_matricula(self, matricula: str):
        """Define os 4 últimos dígitos da matrícula"""
        self.matricula = matricula[-4:].zfill(4)
    
    @staticmethod
    def calcular_crc(data: bytes) -> int:
        """Calcula CRC-16 MODBUS"""
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc >>= 1
        return crc
    
    def _criar_requisicao(self, slave_addr: int, function: int, 
                         data: bytes) -> bytes:
        """Cria requisição MODBUS com matrícula e CRC"""
        # Monta mensagem: endereço + função + dados + matrícula
        mensagem = bytes([slave_addr, function]) + data
        
        # Adiciona matrícula (4 dígitos ASCII)
        mensagem += self.matricula.encode('ascii')
        
        # Calcula e adiciona CRC
        crc = self.calcular_crc(mensagem)
        mensagem += struct.pack('<H', crc)
        
        return mensagem
    
    def _validar_resposta(self, resposta: bytes, slave_addr: int, 
                         function: int) -> bytes:
        """Valida resposta MODBUS"""
        if len(resposta) < 8:  # mínimo: addr + func + dados + matrícula(4) + CRC(2)
            raise ModbusException("Resposta muito curta")
        
        # Valida CRC
        msg_sem_crc = resposta[:-2]
        crc_recebido = struct.unpack('<H', resposta[-2:])[0]
        crc_calculado = self.calcular_crc(msg_sem_crc)
        
        if crc_recebido != crc_calculado:
            raise ModbusException(f"CRC inválido: esperado {crc_calculado:04X}, recebido {crc_recebido:04X}")
        
        # Valida endereço e função
        if resposta[0] != slave_addr:
            raise ModbusException(f"Endereço inválido: {resposta[0]}")
        
        if resposta[1] & 0x80:  # Bit de erro setado
            raise ModbusException(f"Erro MODBUS: código {resposta[2]}")
        
        if resposta[1] != function:
            raise ModbusException(f"Função inválida: {resposta[1]}")
        
        # Remove endereço, função, matrícula e CRC, retorna apenas dados
        return resposta[2:-6]  # Remove addr(1) + func(1) e matrícula(4) + CRC(2) do final
    
    def read_holding_registers(self, slave_addr: int, start_addr: int, 
                               count: int, retries: int = 3) -> Optional[List[int]]:
        """Lê holding registers (função 0x03)"""
        for tentativa in range(retries):
            try:
                # Cria requisição
                data = struct.pack('>HH', start_addr, count)
                requisicao = self._criar_requisicao(
                    slave_addr, ModbusFunction.READ_HOLDING_REGISTERS, data
                )
                
                # Limpa buffer e envia
                self.serial.reset_input_buffer()
                self.serial.write(requisicao)
                
                # Aguarda resposta
                # Tamanho esperado: addr(1) + func(1) + byte_count(1) + data(count*2) + matrícula(4) + CRC(2)
                tamanho_esperado = 1 + 1 + 1 + (count * 2) + 4 + 2
                resposta = self.serial.read(tamanho_esperado)
                
                if len(resposta) == 0:
                    raise ModbusException("Timeout: sem resposta")
                
                # Valida resposta
                dados = self._validar_resposta(resposta, slave_addr, 
                                              ModbusFunction.READ_HOLDING_REGISTERS)
                
                # Extrai valores dos registradores
                byte_count = dados[0]
                valores = []
                for i in range(0, byte_count, 2):
                    valor = struct.unpack('>H', dados[1+i:3+i])[0]
                    valores.append(valor)
                
                logger.debug(f"Lidos {len(valores)} registradores de 0x{slave_addr:02X}")
                return valores
                
            except ModbusException as e:
                logger.warning(f"Tentativa {tentativa+1}/{retries} falhou: {e}")
                if tentativa < retries - 1:
                    time.sleep(0.1 * (tentativa + 1))  # Backoff exponencial
                else:
                    logger.error(f"Falha ao ler registradores após {retries} tentativas")
                    return None
            except Exception as e:
                logger.error(f"Erro inesperado: {e}")
                return None
        
        return None
    
    def write_multiple_registers(self, slave_addr: int, start_addr: int, 
                                 values: List[int], retries: int = 3) -> bool:
        """Escreve múltiplos holding registers (função 0x10)"""
        for tentativa in range(retries):
            try:
                # Cria requisição
                count = len(values)
                byte_count = count * 2
                data = struct.pack('>HHB', start_addr, count, byte_count)
                
                # Adiciona valores
                for valor in values:
                    data += struct.pack('>H', valor)
                
                requisicao = self._criar_requisicao(
                    slave_addr, ModbusFunction.WRITE_MULTIPLE_REGISTERS, data
                )
                
                # Limpa buffer e envia
                self.serial.reset_input_buffer()
                self.serial.write(requisicao)
                
                # Aguarda resposta
                # Tamanho esperado: addr(1) + func(1) + start_addr(2) + count(2) + matrícula(4) + CRC(2)
                tamanho_esperado = 1 + 1 + 2 + 2 + 4 + 2
                resposta = self.serial.read(tamanho_esperado)
                
                if len(resposta) == 0:
                    raise ModbusException("Timeout: sem resposta")
                
                # Valida resposta
                self._validar_resposta(resposta, slave_addr, 
                                      ModbusFunction.WRITE_MULTIPLE_REGISTERS)
                
                logger.debug(f"Escritos {count} registradores em 0x{slave_addr:02X}")
                return True
                
            except ModbusException as e:
                logger.warning(f"Tentativa {tentativa+1}/{retries} falhou: {e}")
                if tentativa < retries - 1:
                    time.sleep(0.1 * (tentativa + 1))
                else:
                    logger.error(f"Falha ao escrever registradores após {retries} tentativas")
                    return False
            except Exception as e:
                logger.error(f"Erro inesperado: {e}")
                return False
        
        return False

class CameraLPR:
    """Interface para Câmera LPR via MODBUS"""
    
    # Offsets dos registradores
    REG_STATUS = 0
    REG_TRIGGER = 1
    REG_PLACA = 2
    REG_CONFIANCA = 6
    REG_ERRO = 7
    
    # Status
    STATUS_PRONTO = 0
    STATUS_PROCESSANDO = 1
    STATUS_OK = 2
    STATUS_ERRO = 3
    
    def __init__(self, modbus: ModbusClient, endereco: int):
        self.modbus = modbus
        self.endereco = endereco
    
    def capturar_placa(self, timeout: float = 2.0) -> Optional[Tuple[str, int]]:
        """
        Captura placa de veículo
        Retorna tupla (placa, confiança) ou None em caso de erro
        """
        try:
            # 1. Dispara captura (escreve 1 no trigger)
            if not self.modbus.write_multiple_registers(self.endereco, self.REG_TRIGGER, [1]):
                logger.error("Falha ao disparar captura")
                return None
            
            # 2. Faz polling do status
            tempo_inicio = time.time()
            while time.time() - tempo_inicio < timeout:
                status_reg = self.modbus.read_holding_registers(self.endereco, self.REG_STATUS, 1)
                if not status_reg:
                    return None
                
                status = status_reg[0]
                
                if status == self.STATUS_OK:
                    # 3. Lê placa e confiança
                    dados = self.modbus.read_holding_registers(self.endereco, self.REG_PLACA, 5)
                    if not dados:
                        return None
                    
                    # Converte registradores para placa (4 regs = 8 chars ASCII)
                    placa_bytes = b''
                    for i in range(4):
                        placa_bytes += struct.pack('>H', dados[i])
                    placa = placa_bytes.decode('ascii').rstrip('\x00')
                    
                    confianca = dados[4]
                    
                    # 4. Zera trigger
                    self.modbus.write_multiple_registers(self.endereco, self.REG_TRIGGER, [0])
                    
                    logger.info(f"Placa capturada: {placa} (confiança: {confianca}%)")
                    return (placa, confianca)
                
                elif status == self.STATUS_ERRO:
                    logger.error("Erro na captura da placa")
                    # Zera trigger
                    self.modbus.write_multiple_registers(self.endereco, self.REG_TRIGGER, [0])
                    return None
                
                # Aguarda um pouco antes de verificar novamente
                time.sleep(0.1)
            
            # Timeout
            logger.error("Timeout ao aguardar captura de placa")
            # Zera trigger
            self.modbus.write_multiple_registers(self.endereco, self.REG_TRIGGER, [0])
            return None
            
        except Exception as e:
            logger.error(f"Erro ao capturar placa: {e}")
            return None

class PlacarVagas:
    """Interface para Placar de Vagas via MODBUS"""
    
    # Offsets dos registradores
    REG_TERREO_PNE = 0
    REG_TERREO_IDOSO = 1
    REG_TERREO_COMUNS = 2
    REG_ANDAR1_PNE = 3
    REG_ANDAR1_IDOSO = 4
    REG_ANDAR1_COMUNS = 5
    REG_ANDAR2_PNE = 6
    REG_ANDAR2_IDOSO = 7
    REG_ANDAR2_COMUNS = 8
    REG_CARROS_TERREO = 9
    REG_CARROS_ANDAR1 = 10
    REG_CARROS_ANDAR2 = 11
    REG_FLAGS = 12
    
    def __init__(self, modbus: ModbusClient, endereco: int):
        self.modbus = modbus
        self.endereco = endereco
    
    def atualizar(self, vagas_livres: dict, num_carros: dict, 
                 lotado_geral: bool = False, 
                 lotado_andar1: bool = False, 
                 lotado_andar2: bool = False) -> bool:
        """
        Atualiza placar com informações de vagas
        
        vagas_livres: {
            'terreo': {'pne': X, 'idoso': Y, 'comuns': Z},
            'andar1': {...},
            'andar2': {...}
        }
        num_carros: {'terreo': X, 'andar1': Y, 'andar2': Z}
        """
        try:
            # Monta lista de valores
            valores = [
                vagas_livres.get('terreo', {}).get('pne', 0),
                vagas_livres.get('terreo', {}).get('idoso', 0),
                vagas_livres.get('terreo', {}).get('comuns', 0),
                vagas_livres.get('andar1', {}).get('pne', 0),
                vagas_livres.get('andar1', {}).get('idoso', 0),
                vagas_livres.get('andar1', {}).get('comuns', 0),
                vagas_livres.get('andar2', {}).get('pne', 0),
                vagas_livres.get('andar2', {}).get('idoso', 0),
                vagas_livres.get('andar2', {}).get('comuns', 0),
                num_carros.get('terreo', 0),
                num_carros.get('andar1', 0),
                num_carros.get('andar2', 0),
            ]
            
            # Calcula flags
            flags = 0
            if lotado_geral:
                flags |= 0x01
            if lotado_andar1:
                flags |= 0x02
            if lotado_andar2:
                flags |= 0x04
            valores.append(flags)
            
            # Escreve no placar
            sucesso = self.modbus.write_multiple_registers(
                self.endereco, self.REG_TERREO_PNE, valores
            )
            
            if sucesso:
                logger.debug("Placar atualizado com sucesso")
            
            return sucesso
            
        except Exception as e:
            logger.error(f"Erro ao atualizar placar: {e}")
            return False

