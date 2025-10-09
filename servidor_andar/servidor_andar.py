"""
Servidor de Andar (1º e 2º Andares)
Gerencia sensores de vaga e passagem entre andares
"""

import asyncio
import logging
import sys
from pathlib import Path
from typing import Optional, Dict, Any

sys.path.append(str(Path(__file__).parent.parent))

from comum.config import Config
from comum.mensagens import Mensagem, TipoMensagem
from comum.comunicacao import ServidorTCP, ClienteTCP
from comum.gpio_handler import GPIOHandler, VarredorVagas, SensorPassagem

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Configuração GPIO do 1º Andar (Tabela 2)
GPIO_ANDAR1 = {
    'endereco_01': 16,
    'endereco_02': 20,
    'endereco_03': 21,
    'sensor_vaga': 27,
    'sensor_passagem_1': 22,
    'sensor_passagem_2': 11
}

# Configuração GPIO do 2º Andar (Tabela 3)
GPIO_ANDAR2 = {
    'endereco_01': 0,
    'endereco_02': 5,
    'endereco_03': 6,
    'sensor_vaga': 13,
    'sensor_passagem_1': 19,
    'sensor_passagem_2': 26
}

class ServidorAndar:
    """Servidor de Andar (1º ou 2º)"""
    
    def __init__(self, config: Config, numero_andar: int):
        self.config = config
        self.numero_andar = numero_andar
        self.nome_andar = f"andar{numero_andar}"
        
        # Seleciona configuração GPIO apropriada
        if numero_andar == 1:
            gpio_config = GPIO_ANDAR1
            self.vagas_config = {
                'pne': config.get('vagas_andar1_pne', 2),
                'idoso': config.get('vagas_andar1_idoso', 2),
                'comuns': config.get('vagas_andar1_comuns', 4)
            }
            self.host = config.get('servidor_andar1_host', '0.0.0.0')
            self.port = config.get('servidor_andar1_port', 5002)
        else:  # andar 2
            gpio_config = GPIO_ANDAR2
            self.vagas_config = {
                'pne': config.get('vagas_andar2_pne', 2),
                'idoso': config.get('vagas_andar2_idoso', 2),
                'comuns': config.get('vagas_andar2_comuns', 4)
            }
            self.host = config.get('servidor_andar2_host', '0.0.0.0')
            self.port = config.get('servidor_andar2_port', 5003)
        
        # GPIO
        self.gpio = GPIOHandler()
        
        # Varredor de vagas (3 bits de endereço = 8 vagas)
        self.varredor = VarredorVagas(
            self.gpio,
            [gpio_config['endereco_01'], gpio_config['endereco_02'], gpio_config['endereco_03']],
            gpio_config['sensor_vaga']
        )
        
        # Sensor de passagem entre andares
        self.sensor_passagem = SensorPassagem(
            self.gpio,
            gpio_config['sensor_passagem_1'],
            gpio_config['sensor_passagem_2'],
            self._callback_subindo,
            self._callback_descendo
        )
        
        # Comunicação
        self.servidor_tcp = None
        self.cliente_central = None
        
        # Estado das vagas
        self.vagas_ocupadas = [False] * self.varredor.num_vagas
        self.vagas_livres = dict(self.vagas_config)
        
        # Tarefas assíncronas
        self.tarefas = []
        self.running = False
    
    def _callback_subindo(self):
        """Callback quando veículo sobe (1→2)"""
        if self.numero_andar == 1:
            logger.info("Veículo subindo para o 2º andar")
            asyncio.create_task(self._enviar_passagem("subindo"))
    
    def _callback_descendo(self):
        """Callback quando veículo desce (2→1)"""
        if self.numero_andar == 1:
            logger.info("Veículo descendo para o 1º andar")
            asyncio.create_task(self._enviar_passagem("descendo"))
    
    async def _enviar_passagem(self, direcao: str):
        """Envia evento de passagem ao servidor central"""
        if self.cliente_central:
            mensagem = Mensagem.criar_passagem_andar(direcao)
            await self.cliente_central.enviar(mensagem)
    
    async def processar_mensagem(self, mensagem: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Processa mensagem recebida"""
        tipo = mensagem.get('tipo')
        
        logger.debug(f"Processando mensagem: {tipo}")
        
        # Por enquanto, apenas responde OK
        return {"status": "ok"}
    
    async def tarefa_varredura_vagas(self):
        """Tarefa de varredura periódica de vagas"""
        logger.info(f"Tarefa de varredura de vagas do {self.nome_andar} iniciada")
        
        while self.running:
            try:
                # Varre todas as vagas
                estados = self.varredor.varrer_todas()
                
                # Detecta mudanças
                mudancas = []
                for i, ocupada in enumerate(estados):
                    if ocupada != self.vagas_ocupadas[i]:
                        mudancas.append((i, ocupada))
                        self.vagas_ocupadas[i] = ocupada
                
                # Processa mudanças
                if mudancas:
                    logger.info(f"Mudanças detectadas no {self.nome_andar}: {mudancas}")
                    
                    # Calcula vagas livres por tipo (simplificado)
                    # Em produção, seria necessário mapear cada vaga para seu tipo
                    total_livres = sum(1 for ocupada in estados if not ocupada)
                    
                    # Distribui proporcionalmente (simplificado)
                    total_vagas = sum(self.vagas_config.values())
                    if total_vagas > 0:
                        for tipo in ['pne', 'idoso', 'comuns']:
                            proporcao = self.vagas_config[tipo] / total_vagas
                            self.vagas_livres[tipo] = int(total_livres * proporcao)
                    
                    # Envia atualização ao servidor central
                    if self.cliente_central:
                        mensagem = Mensagem.criar_vaga_status(
                            self.numero_andar, 
                            self.vagas_livres
                        )
                        await self.cliente_central.enviar(mensagem)
                
                # Aguarda antes da próxima varredura
                await asyncio.sleep(1.0)
                
            except Exception as e:
                logger.error(f"Erro na varredura de vagas: {e}")
                await asyncio.sleep(1.0)
    
    async def iniciar(self):
        """Inicia o servidor do andar"""
        logger.info(f"Iniciando Servidor do {self.numero_andar}º Andar...")
        
        self.running = True
        
        # Configura cliente para servidor central
        host_central = self.config.get('servidor_central_host', 'localhost')
        port_central = self.config.get('servidor_central_port', 5000)
        self.cliente_central = ClienteTCP(host_central, port_central)
        
        # Aguarda servidor central iniciar
        await asyncio.sleep(1)
        
        # Inicia servidor TCP
        self.servidor_tcp = ServidorTCP(self.host, self.port, self.processar_mensagem)
        
        logger.info(f"Servidor do {self.numero_andar}º Andar iniciado em {self.host}:{self.port}")
        
        # Inicia tarefas assíncronas
        self.tarefas = [
            asyncio.create_task(self.tarefa_varredura_vagas()),
            asyncio.create_task(self.servidor_tcp.iniciar())
        ]
        
        # Aguarda tarefas
        try:
            await asyncio.gather(*self.tarefas)
        except asyncio.CancelledError:
            logger.info("Tarefas canceladas")
    
    async def parar(self):
        """Para o servidor"""
        logger.info(f"Parando Servidor do {self.numero_andar}º Andar...")
        self.running = False
        
        # Cancela tarefas
        for tarefa in self.tarefas:
            tarefa.cancel()
        
        # Para servidor TCP
        if self.servidor_tcp:
            await self.servidor_tcp.parar()
        
        # Limpa GPIO
        self.gpio.cleanup()
        
        logger.info(f"Servidor do {self.numero_andar}º Andar parado")

async def main():
    """Função principal"""
    import sys
    
    # Verifica argumento para saber qual andar
    if len(sys.argv) < 2:
        print("Uso: python servidor_andar.py [1|2]")
        print("  1 - 1º Andar")
        print("  2 - 2º Andar")
        sys.exit(1)
    
    numero_andar = int(sys.argv[1])
    
    if numero_andar not in [1, 2]:
        print("Erro: número do andar deve ser 1 ou 2")
        sys.exit(1)
    
    # Carrega configuração
    config = Config()
    
    # Cria e inicia servidor
    servidor = ServidorAndar(config, numero_andar)
    
    try:
        await servidor.iniciar()
    except KeyboardInterrupt:
        logger.info(f"Encerrando Servidor do {numero_andar}º Andar...")
        await servidor.parar()
    except Exception as e:
        logger.error(f"Erro: {e}")
        await servidor.parar()

if __name__ == "__main__":
    asyncio.run(main())

