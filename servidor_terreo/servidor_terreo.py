"""
Servidor do Andar Térreo
Gerencia cancelas, sensores de vaga e comunicação MODBUS
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
from comum.gpio_handler import GPIOHandler, VarredorVagas, ControleCancela
from comum.modbus_client import ModbusClient, CameraLPR, PlacarVagas

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Configuração GPIO do Térreo (Tabela 1)
GPIO_TERREO = {
    'endereco_01': 17,
    'endereco_02': 18,
    'sensor_vaga': 8,
    'sensor_abertura_cancela_entrada': 7,
    'sensor_fechamento_cancela_entrada': 1,
    'motor_cancela_entrada': 23,
    'sensor_abertura_cancela_saida': 12,
    'sensor_fechamento_cancela_saida': 25,
    'motor_cancela_saida': 24
}

class ServidorTerreo:
    """Servidor do Andar Térreo"""
    
    def __init__(self, config: Config):
        self.config = config
        
        # GPIO
        self.gpio = GPIOHandler()
        
        # Varredor de vagas (2 bits de endereço = 4 vagas)
        self.varredor = VarredorVagas(
            self.gpio,
            [GPIO_TERREO['endereco_01'], GPIO_TERREO['endereco_02']],
            GPIO_TERREO['sensor_vaga']
        )
        
        # Cancelas
        self.cancela_entrada = ControleCancela(
            self.gpio,
            GPIO_TERREO['motor_cancela_entrada'],
            GPIO_TERREO['sensor_abertura_cancela_entrada'],
            GPIO_TERREO['sensor_fechamento_cancela_entrada'],
            "entrada"
        )
        
        self.cancela_saida = ControleCancela(
            self.gpio,
            GPIO_TERREO['motor_cancela_saida'],
            GPIO_TERREO['sensor_abertura_cancela_saida'],
            GPIO_TERREO['sensor_fechamento_cancela_saida'],
            "saida"
        )
        
        # MODBUS
        try:
            self.modbus = ModbusClient(
                config.get('modbus_port', '/dev/ttyUSB0'),
                config.get('modbus_baudrate', 115200),
                config.get('modbus_timeout', 0.5)
            )
            self.modbus.set_matricula(config.get('matricula', '1606'))
            self.modbus.conectar()
            
            # Câmeras LPR
            self.camera_entrada = CameraLPR(
                self.modbus,
                config.get('modbus_camera_entrada', 0x11)
            )
            
            self.camera_saida = CameraLPR(
                self.modbus,
                config.get('modbus_camera_saida', 0x12)
            )
            
            # Placar
            self.placar = PlacarVagas(
                self.modbus,
                config.get('modbus_placar', 0x20)
            )
            
            self.modbus_ativo = True
            logger.info("MODBUS inicializado com sucesso")
            
        except Exception as e:
            logger.error(f"Erro ao inicializar MODBUS: {e}")
            logger.warning("Sistema operará em modo degradado (sem MODBUS)")
            self.modbus_ativo = False
        
        # Comunicação
        self.servidor_tcp = None
        self.cliente_central = None
        
        # Estado das vagas
        self.vagas_ocupadas = [False] * self.varredor.num_vagas
        self.vagas_livres = {
            'pne': config.get('vagas_terreo_pne', 2),
            'idoso': config.get('vagas_terreo_idoso', 2),
            'comuns': config.get('vagas_terreo_comuns', 4)
        }
        
        # Configuração
        self.confianca_minima = config.get('confianca_minima', 70)
        
        # Tarefas assíncronas
        self.tarefas = []
        self.running = False
    
    async def processar_mensagem(self, mensagem: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Processa mensagem recebida"""
        tipo = mensagem.get('tipo')
        
        logger.info(f"Processando mensagem: {tipo}")
        
        if tipo == TipoMensagem.COMANDO_CANCELA:
            return await self._processar_comando_cancela(mensagem)
        
        elif tipo == TipoMensagem.ATUALIZAR_PLACAR:
            return await self._processar_atualizar_placar(mensagem)
        
        return {"status": "ok"}
    
    async def _processar_comando_cancela(self, mensagem: Dict[str, Any]) -> Dict[str, Any]:
        """Processa comando de cancela"""
        cancela = mensagem.get('cancela')
        acao = mensagem.get('acao')
        
        controle = None
        if cancela == "entrada":
            controle = self.cancela_entrada
        elif cancela == "saida":
            controle = self.cancela_saida
        
        if not controle:
            return {"status": "erro", "motivo": "cancela_invalida"}
        
        if acao == "abrir":
            sucesso = controle.abrir()
        elif acao == "fechar":
            sucesso = controle.fechar()
        else:
            return {"status": "erro", "motivo": "acao_invalida"}
        
        return {"status": "ok" if sucesso else "erro"}
    
    async def _processar_atualizar_placar(self, mensagem: Dict[str, Any]) -> Dict[str, Any]:
        """Processa atualização do placar"""
        if not self.modbus_ativo:
            return {"status": "erro", "motivo": "modbus_inativo"}
        
        dados = mensagem.get('dados', {})
        vagas_livres = dados.get('vagas_livres', {})
        num_carros = dados.get('num_carros', {})
        
        lotado_geral = dados.get('lotado_geral', False)
        lotado_andar1 = dados.get('lotado_andar1', False)
        lotado_andar2 = dados.get('lotado_andar2', False)
        
        sucesso = self.placar.atualizar(
            vagas_livres, num_carros,
            lotado_geral, lotado_andar1, lotado_andar2
        )
        
        return {"status": "ok" if sucesso else "erro"}
    
    async def tarefa_varredura_vagas(self):
        """Tarefa de varredura periódica de vagas"""
        logger.info("Tarefa de varredura de vagas iniciada")
        
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
                    logger.info(f"Mudanças detectadas: {mudancas}")
                    
                    # Calcula vagas livres (simplificado)
                    total_livres = sum(1 for ocupada in estados if not ocupada)
                    
                    # Envia atualização ao servidor central
                    if self.cliente_central:
                        mensagem = Mensagem.criar_vaga_status(0, self.vagas_livres)
                        await self.cliente_central.enviar(mensagem)
                
                # Aguarda antes da próxima varredura
                await asyncio.sleep(1.0)
                
            except Exception as e:
                logger.error(f"Erro na varredura de vagas: {e}")
                await asyncio.sleep(1.0)
    
    async def tarefa_cancela_entrada(self):
        """Tarefa de gerenciamento da cancela de entrada"""
        logger.info("Tarefa de cancela de entrada iniciada")
        
        # Configura interrupção no sensor de presença
        sensor_presenca = GPIO_TERREO['sensor_abertura_cancela_entrada']
        
        while self.running:
            try:
                # Verifica se há veículo no sensor de presença
                if self.gpio.ler(sensor_presenca):
                    logger.info("Veículo detectado na entrada")
                    
                    # Captura placa via MODBUS
                    placa = None
                    confianca = 0
                    
                    if self.modbus_ativo:
                        resultado = self.camera_entrada.capturar_placa()
                        if resultado:
                            placa, confianca = resultado
                    
                    # Se não conseguiu ler placa ou baixa confiança, gera ID temporário
                    if not placa or confianca < self.confianca_minima:
                        if not placa:
                            logger.warning("Falha ao capturar placa, usando ID temporário")
                            placa = f"TEMP{asyncio.get_event_loop().time():.0f}"
                            confianca = 0
                        else:
                            logger.warning(f"Baixa confiança ({confianca}%), mas permitindo entrada")
                    
                    # Envia evento ao servidor central
                    if self.cliente_central:
                        mensagem = Mensagem.criar_entrada_ok(placa, confianca, 0)
                        resposta = await self.cliente_central.enviar(mensagem, esperar_resposta=True)
                        
                        if resposta and resposta.get('status') == 'ok':
                            # Abre cancela
                            logger.info("Abrindo cancela de entrada")
                            self.cancela_entrada.abrir()
                            
                            # Aguarda veículo passar (sensor de abertura ficar livre)
                            tempo_espera = 0
                            while self.gpio.ler(sensor_presenca) and tempo_espera < 30:
                                await asyncio.sleep(0.5)
                                tempo_espera += 0.5
                            
                            # Aguarda um pouco mais
                            await asyncio.sleep(2)
                            
                            # Fecha cancela
                            logger.info("Fechando cancela de entrada")
                            self.cancela_entrada.fechar()
                        else:
                            logger.warning("Entrada não autorizada pelo servidor central")
                    
                    # Aguarda sensor liberar
                    while self.gpio.ler(sensor_presenca):
                        await asyncio.sleep(0.5)
                
                await asyncio.sleep(0.5)
                
            except Exception as e:
                logger.error(f"Erro na cancela de entrada: {e}")
                await asyncio.sleep(1.0)
    
    async def tarefa_cancela_saida(self):
        """Tarefa de gerenciamento da cancela de saída"""
        logger.info("Tarefa de cancela de saída iniciada")
        
        # Configura interrupção no sensor de presença
        sensor_presenca = GPIO_TERREO['sensor_abertura_cancela_saida']
        
        while self.running:
            try:
                # Verifica se há veículo no sensor de presença
                if self.gpio.ler(sensor_presenca):
                    logger.info("Veículo detectado na saída")
                    
                    # Captura placa via MODBUS
                    placa = None
                    confianca = 0
                    
                    if self.modbus_ativo:
                        resultado = self.camera_saida.capturar_placa()
                        if resultado:
                            placa, confianca = resultado
                    
                    # Se não conseguiu ler placa, permite saída
                    if not placa:
                        logger.warning("Falha ao capturar placa na saída, liberando")
                        placa = "DESCONHECIDO"
                        confianca = 0
                    
                    # Envia evento ao servidor central e solicita cálculo
                    if self.cliente_central:
                        mensagem = Mensagem.criar_saida_ok(placa, confianca)
                        resposta = await self.cliente_central.enviar(mensagem, esperar_resposta=True)
                        
                        if resposta and resposta.get('status') == 'ok':
                            # Exibe valor
                            valor = resposta.get('valor', 0.0)
                            tempo = resposta.get('tempo_minutos', 0)
                            
                            logger.info(f"Valor a pagar: R$ {valor:.2f} ({tempo} minutos)")
                            print(f"\n{'='*50}")
                            print(f"RECIBO DE PAGAMENTO")
                            print(f"{'='*50}")
                            print(f"Placa: {placa}")
                            print(f"Tempo: {tempo} minutos")
                            print(f"Valor: R$ {valor:.2f}")
                            print(f"{'='*50}\n")
                            
                            # Abre cancela
                            logger.info("Abrindo cancela de saída")
                            self.cancela_saida.abrir()
                            
                            # Aguarda veículo passar
                            tempo_espera = 0
                            while self.gpio.ler(sensor_presenca) and tempo_espera < 30:
                                await asyncio.sleep(0.5)
                                tempo_espera += 0.5
                            
                            # Aguarda um pouco mais
                            await asyncio.sleep(2)
                            
                            # Fecha cancela
                            logger.info("Fechando cancela de saída")
                            self.cancela_saida.fechar()
                    
                    # Aguarda sensor liberar
                    while self.gpio.ler(sensor_presenca):
                        await asyncio.sleep(0.5)
                
                await asyncio.sleep(0.5)
                
            except Exception as e:
                logger.error(f"Erro na cancela de saída: {e}")
                await asyncio.sleep(1.0)
    
    async def iniciar(self):
        """Inicia o servidor do térreo"""
        logger.info("Iniciando Servidor do Térreo...")
        
        self.running = True
        
        # Configura cliente para servidor central
        host_central = self.config.get('servidor_central_host', 'localhost')
        port_central = self.config.get('servidor_central_port', 5000)
        self.cliente_central = ClienteTCP(host_central, port_central)
        
        # Aguarda servidor central iniciar
        await asyncio.sleep(1)
        
        # Inicia servidor TCP
        host = self.config.get('servidor_terreo_host', '0.0.0.0')
        port = self.config.get('servidor_terreo_port', 5001)
        self.servidor_tcp = ServidorTCP(host, port, self.processar_mensagem)
        
        logger.info(f"Servidor Térreo iniciado em {host}:{port}")
        
        # Inicia tarefas assíncronas
        self.tarefas = [
            asyncio.create_task(self.tarefa_varredura_vagas()),
            asyncio.create_task(self.tarefa_cancela_entrada()),
            asyncio.create_task(self.tarefa_cancela_saida()),
            asyncio.create_task(self.servidor_tcp.iniciar())
        ]
        
        # Aguarda tarefas
        try:
            await asyncio.gather(*self.tarefas)
        except asyncio.CancelledError:
            logger.info("Tarefas canceladas")
    
    async def parar(self):
        """Para o servidor"""
        logger.info("Parando Servidor do Térreo...")
        self.running = False
        
        # Cancela tarefas
        for tarefa in self.tarefas:
            tarefa.cancel()
        
        # Para servidor TCP
        if self.servidor_tcp:
            await self.servidor_tcp.parar()
        
        # Desconecta MODBUS
        if self.modbus_ativo:
            self.modbus.desconectar()
        
        # Limpa GPIO
        self.gpio.cleanup()
        
        logger.info("Servidor do Térreo parado")

async def main():
    """Função principal"""
    # Carrega configuração
    config = Config()
    
    # Cria e inicia servidor
    servidor = ServidorTerreo(config)
    
    try:
        await servidor.iniciar()
    except KeyboardInterrupt:
        logger.info("Encerrando Servidor do Térreo...")
        await servidor.parar()
    except Exception as e:
        logger.error(f"Erro: {e}")
        await servidor.parar()

if __name__ == "__main__":
    asyncio.run(main())

