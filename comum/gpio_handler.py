"""
Módulo de Gerenciamento de GPIO
Gerencia sensores e atuadores via GPIO da Raspberry Pi
"""

import logging
import time
from typing import Callable, Optional, List
try:
    import RPi.GPIO as GPIO
except (ImportError, RuntimeError):
    # Fallback para desenvolvimento sem Raspberry Pi
    class GPIO:
        BCM = 'BCM'
        OUT = 'OUT'
        IN = 'IN'
        PUD_DOWN = 'PUD_DOWN'
        PUD_UP = 'PUD_UP'
        HIGH = 1
        LOW = 0
        RISING = 'RISING'
        FALLING = 'FALLING'
        BOTH = 'BOTH'
        
        @staticmethod
        def setmode(mode): pass
        @staticmethod
        def setup(pin, mode, pull_up_down=None): pass
        @staticmethod
        def output(pin, value): pass
        @staticmethod
        def input(pin): return 0
        @staticmethod
        def add_event_detect(pin, edge, callback=None, bouncetime=None): pass
        @staticmethod
        def remove_event_detect(pin): pass
        @staticmethod
        def cleanup(): pass

logger = logging.getLogger(__name__)

class GPIOHandler:
    """Classe base para gerenciamento de GPIO"""
    
    def __init__(self):
        GPIO.setmode(GPIO.BCM)
        self.pinos_configurados = []
    
    def configurar_saida(self, pino: int, inicial: int = GPIO.LOW):
        """Configura pino como saída"""
        GPIO.setup(pino, GPIO.OUT, initial=inicial)
        self.pinos_configurados.append(pino)
        logger.debug(f"Pino {pino} configurado como saída")
    
    def configurar_entrada(self, pino: int, pull_up_down=GPIO.PUD_DOWN):
        """Configura pino como entrada"""
        GPIO.setup(pino, GPIO.IN, pull_up_down=pull_up_down)
        self.pinos_configurados.append(pino)
        logger.debug(f"Pino {pino} configurado como entrada")
    
    def escrever(self, pino: int, valor: int):
        """Escreve valor em pino de saída"""
        GPIO.output(pino, valor)
    
    def ler(self, pino: int) -> int:
        """Lê valor de pino de entrada"""
        return GPIO.input(pino)
    
    def adicionar_interrupcao(self, pino: int, callback: Callable, 
                             edge=GPIO.BOTH, bouncetime: int = 200):
        """Adiciona interrupção a pino de entrada"""
        GPIO.add_event_detect(pino, edge, callback=callback, bouncetime=bouncetime)
        logger.debug(f"Interrupção adicionada ao pino {pino}")
    
    def remover_interrupcao(self, pino: int):
        """Remove interrupção de pino"""
        try:
            GPIO.remove_event_detect(pino)
            logger.debug(f"Interrupção removida do pino {pino}")
        except:
            pass
    
    def cleanup(self):
        """Limpa configurações de GPIO"""
        GPIO.cleanup()
        logger.info("GPIO cleanup realizado")

class VarredorVagas:
    """Gerencia varredura multiplexada de vagas"""
    
    def __init__(self, gpio: GPIOHandler, pinos_endereco: List[int], 
                 pino_sensor: int):
        self.gpio = gpio
        self.pinos_endereco = pinos_endereco
        self.pino_sensor = pino_sensor
        self.num_vagas = 2 ** len(pinos_endereco)
        
        # Configura pinos
        for pino in pinos_endereco:
            self.gpio.configurar_saida(pino)
        self.gpio.configurar_entrada(pino_sensor)
        
        logger.info(f"Varredor de vagas configurado para {self.num_vagas} vagas")
    
    def selecionar_endereco(self, endereco: int):
        """Seleciona endereço para leitura"""
        for i, pino in enumerate(self.pinos_endereco):
            bit = (endereco >> i) & 1
            self.gpio.escrever(pino, GPIO.HIGH if bit else GPIO.LOW)
        
        # Aguarda estabilização
        time.sleep(0.001)
    
    def ler_vaga(self, endereco: int) -> bool:
        """Lê estado de uma vaga (True = ocupada, False = livre)"""
        self.selecionar_endereco(endereco)
        return self.gpio.ler(self.pino_sensor) == GPIO.HIGH
    
    def varrer_todas(self) -> List[bool]:
        """Varre todas as vagas e retorna lista de estados"""
        estados = []
        for endereco in range(self.num_vagas):
            ocupada = self.ler_vaga(endereco)
            estados.append(ocupada)
        return estados

class ControleCancela:
    """Controla cancela de entrada/saída"""
    
    ESTADO_FECHADA = "fechada"
    ESTADO_ABRINDO = "abrindo"
    ESTADO_ABERTA = "aberta"
    ESTADO_FECHANDO = "fechando"
    
    def __init__(self, gpio: GPIOHandler, pino_motor: int, 
                 pino_sensor_abertura: int, pino_sensor_fechamento: int,
                 nome: str = "cancela"):
        self.gpio = gpio
        self.pino_motor = pino_motor
        self.pino_sensor_abertura = pino_sensor_abertura
        self.pino_sensor_fechamento = pino_sensor_fechamento
        self.nome = nome
        
        # Configura pinos
        self.gpio.configurar_saida(pino_motor)
        self.gpio.configurar_entrada(pino_sensor_abertura)
        self.gpio.configurar_entrada(pino_sensor_fechamento)
        
        # Estado inicial
        if self.gpio.ler(pino_sensor_fechamento):
            self.estado = self.ESTADO_FECHADA
        elif self.gpio.ler(pino_sensor_abertura):
            self.estado = self.ESTADO_ABERTA
        else:
            self.estado = self.ESTADO_FECHADA
        
        logger.info(f"Cancela '{nome}' inicializada no estado: {self.estado}")
    
    def abrir(self, timeout: float = 10.0) -> bool:
        """Abre a cancela"""
        if self.estado == self.ESTADO_ABERTA:
            logger.info(f"Cancela '{self.nome}' já está aberta")
            return True
        
        logger.info(f"Abrindo cancela '{self.nome}'")
        self.estado = self.ESTADO_ABRINDO
        
        # Liga motor
        self.gpio.escrever(self.pino_motor, GPIO.HIGH)
        
        # Aguarda sensor de abertura
        tempo_inicio = time.time()
        while time.time() - tempo_inicio < timeout:
            if self.gpio.ler(self.pino_sensor_abertura):
                # Cancela aberta
                self.gpio.escrever(self.pino_motor, GPIO.LOW)
                self.estado = self.ESTADO_ABERTA
                logger.info(f"Cancela '{self.nome}' aberta")
                return True
            time.sleep(0.1)
        
        # Timeout
        self.gpio.escrever(self.pino_motor, GPIO.LOW)
        logger.error(f"Timeout ao abrir cancela '{self.nome}'")
        return False
    
    def fechar(self, timeout: float = 10.0) -> bool:
        """Fecha a cancela"""
        if self.estado == self.ESTADO_FECHADA:
            logger.info(f"Cancela '{self.nome}' já está fechada")
            return True
        
        logger.info(f"Fechando cancela '{self.nome}'")
        self.estado = self.ESTADO_FECHANDO
        
        # Liga motor (mesmo sinal, direção controlada por mecânica)
        self.gpio.escrever(self.pino_motor, GPIO.HIGH)
        
        # Aguarda sensor de fechamento
        tempo_inicio = time.time()
        while time.time() - tempo_inicio < timeout:
            if self.gpio.ler(self.pino_sensor_fechamento):
                # Cancela fechada
                self.gpio.escrever(self.pino_motor, GPIO.LOW)
                self.estado = self.ESTADO_FECHADA
                logger.info(f"Cancela '{self.nome}' fechada")
                return True
            time.sleep(0.1)
        
        # Timeout
        self.gpio.escrever(self.pino_motor, GPIO.LOW)
        logger.error(f"Timeout ao fechar cancela '{self.nome}'")
        return False
    
    def obter_estado(self) -> str:
        """Retorna estado atual da cancela"""
        return self.estado

class SensorPassagem:
    """Gerencia sensores de passagem entre andares"""
    
    def __init__(self, gpio: GPIOHandler, pino_sensor1: int, pino_sensor2: int,
                 callback_subindo: Optional[Callable] = None,
                 callback_descendo: Optional[Callable] = None):
        self.gpio = gpio
        self.pino_sensor1 = pino_sensor1
        self.pino_sensor2 = pino_sensor2
        self.callback_subindo = callback_subindo
        self.callback_descendo = callback_descendo
        
        # Configura pinos
        self.gpio.configurar_entrada(pino_sensor1)
        self.gpio.configurar_entrada(pino_sensor2)
        
        # Estado da sequência
        self.ultima_ativacao = None
        self.tempo_ultima_ativacao = 0
        self.timeout_sequencia = 5.0  # segundos
        
        # Adiciona interrupções
        self.gpio.adicionar_interrupcao(pino_sensor1, self._sensor1_callback, GPIO.RISING)
        self.gpio.adicionar_interrupcao(pino_sensor2, self._sensor2_callback, GPIO.RISING)
        
        logger.info("Sensor de passagem configurado")
    
    def _sensor1_callback(self, channel):
        """Callback do sensor 1"""
        tempo_atual = time.time()
        
        # Verifica se é continuação de sequência
        if (self.ultima_ativacao == 2 and 
            tempo_atual - self.tempo_ultima_ativacao < self.timeout_sequencia):
            # Sequência 2 -> 1: descendo
            logger.info("Detecção: veículo descendo (2 → 1)")
            if self.callback_descendo:
                self.callback_descendo()
            self.ultima_ativacao = None
        else:
            # Início de nova sequência
            self.ultima_ativacao = 1
            self.tempo_ultima_ativacao = tempo_atual
    
    def _sensor2_callback(self, channel):
        """Callback do sensor 2"""
        tempo_atual = time.time()
        
        # Verifica se é continuação de sequência
        if (self.ultima_ativacao == 1 and 
            tempo_atual - self.tempo_ultima_ativacao < self.timeout_sequencia):
            # Sequência 1 -> 2: subindo
            logger.info("Detecção: veículo subindo (1 → 2)")
            if self.callback_subindo:
                self.callback_subindo()
            self.ultima_ativacao = None
        else:
            # Início de nova sequência
            self.ultima_ativacao = 2
            self.tempo_ultima_ativacao = tempo_atual

