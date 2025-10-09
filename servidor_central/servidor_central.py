"""
Servidor Central
Consolida informações, calcula cobranças e gerencia o estacionamento
"""

import asyncio
import logging
import sys
from datetime import datetime
from typing import Dict, Optional, Any
from pathlib import Path

# Adiciona diretório raiz ao path
sys.path.append(str(Path(__file__).parent.parent))

from comum.config import Config
from comum.mensagens import Mensagem, TipoMensagem
from comum.comunicacao import ServidorTCP, ClienteTCP

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class RegistroVeiculo:
    """Registro de veículo no estacionamento"""
    
    def __init__(self, placa: str, timestamp_entrada: str, andar: int, confianca: int):
        self.placa = placa
        self.timestamp_entrada = timestamp_entrada
        self.andar_atual = andar
        self.confianca_entrada = confianca
        self.timestamp_saida = None
        self.confianca_saida = None
        self.valor_pago = 0.0
        self.tempo_minutos = 0
    
    def registrar_saida(self, timestamp_saida: str, confianca: int):
        """Registra saída do veículo"""
        self.timestamp_saida = timestamp_saida
        self.confianca_saida = confianca

class ServidorCentral:
    """Servidor Central do Sistema de Estacionamento"""
    
    def __init__(self, config: Config):
        self.config = config
        self.veiculos_ativos = {}  # placa -> RegistroVeiculo
        self.historico = []  # Lista de registros completos
        
        # Contadores de vagas por andar
        self.vagas_totais = {
            'terreo': {
                'pne': config.get('vagas_terreo_pne', 2),
                'idoso': config.get('vagas_terreo_idoso', 2),
                'comuns': config.get('vagas_terreo_comuns', 4)
            },
            'andar1': {
                'pne': config.get('vagas_andar1_pne', 2),
                'idoso': config.get('vagas_andar1_idoso', 2),
                'comuns': config.get('vagas_andar1_comuns', 4)
            },
            'andar2': {
                'pne': config.get('vagas_andar2_pne', 2),
                'idoso': config.get('vagas_andar2_idoso', 2),
                'comuns': config.get('vagas_andar2_comuns', 4)
            }
        }
        
        self.vagas_livres = {
            'terreo': dict(self.vagas_totais['terreo']),
            'andar1': dict(self.vagas_totais['andar1']),
            'andar2': dict(self.vagas_totais['andar2'])
        }
        
        self.num_carros = {'terreo': 0, 'andar1': 0, 'andar2': 0}
        
        # Estados
        self.estacionamento_fechado = False
        self.andar1_bloqueado = False
        self.andar2_bloqueado = False
        
        # Comunicação
        self.servidor_tcp = None
        self.cliente_terreo = None
        
        # Preço
        self.preco_por_minuto = config.get('preco_por_minuto', 0.15)
    
    async def processar_mensagem(self, mensagem: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Processa mensagem recebida"""
        tipo = mensagem.get('tipo')
        
        logger.info(f"Processando mensagem: {tipo}")
        
        if tipo == TipoMensagem.ENTRADA_OK:
            return await self._processar_entrada(mensagem)
        
        elif tipo == TipoMensagem.SAIDA_OK:
            return await self._processar_saida(mensagem)
        
        elif tipo == TipoMensagem.VAGA_STATUS:
            return await self._processar_vaga_status(mensagem)
        
        elif tipo == TipoMensagem.CALCULAR_VALOR:
            return await self._calcular_valor(mensagem)
        
        elif tipo == TipoMensagem.PASSAGEM_ANDAR:
            return await self._processar_passagem_andar(mensagem)
        
        return {"status": "ok"}
    
    async def _processar_entrada(self, mensagem: Dict[str, Any]) -> Dict[str, Any]:
        """Processa entrada de veículo"""
        placa = mensagem.get('placa')
        confianca = mensagem.get('conf', 0)
        timestamp = mensagem.get('ts')
        andar = self._andar_numero_para_nome(mensagem.get('andar', 0))
        
        # Verifica se estacionamento está fechado
        if self.estacionamento_fechado:
            logger.warning("Entrada bloqueada: estacionamento fechado")
            return {"status": "erro", "motivo": "estacionamento_fechado"}
        
        # Verifica se há vagas disponíveis
        if self._esta_lotado():
            logger.warning("Entrada bloqueada: estacionamento lotado")
            return {"status": "erro", "motivo": "lotado"}
        
        # Registra veículo
        registro = RegistroVeiculo(placa, timestamp, andar, confianca)
        self.veiculos_ativos[placa] = registro
        
        # Atualiza contadores
        self.num_carros[andar] += 1
        
        logger.info(f"Veículo {placa} entrou no {andar} (confiança: {confianca}%)")
        
        # Atualiza placar
        await self._atualizar_placar()
        
        return {"status": "ok", "mensagem": "Entrada autorizada"}
    
    async def _processar_saida(self, mensagem: Dict[str, Any]) -> Dict[str, Any]:
        """Processa saída de veículo"""
        placa = mensagem.get('placa')
        confianca = mensagem.get('conf', 0)
        timestamp = mensagem.get('ts')
        
        # Busca veículo
        if placa not in self.veiculos_ativos:
            logger.warning(f"Veículo {placa} não encontrado no sistema")
            # Permite saída mesmo sem registro (falha de entrada)
            return {
                "status": "ok",
                "mensagem": "Saída autorizada (sem registro de entrada)",
                "valor": 0.0
            }
        
        registro = self.veiculos_ativos[placa]
        registro.registrar_saida(timestamp, confianca)
        
        # Calcula valor
        valor, tempo_minutos = self._calcular_cobranca(
            registro.timestamp_entrada, timestamp
        )
        registro.valor_pago = valor
        registro.tempo_minutos = tempo_minutos
        
        # Atualiza contadores
        self.num_carros[registro.andar_atual] -= 1
        
        # Move para histórico
        self.historico.append(registro)
        del self.veiculos_ativos[placa]
        
        logger.info(f"Veículo {placa} saiu. Tempo: {tempo_minutos}min, Valor: R$ {valor:.2f}")
        
        # Atualiza placar
        await self._atualizar_placar()
        
        return {
            "status": "ok",
            "mensagem": "Saída autorizada",
            "valor": valor,
            "tempo_minutos": tempo_minutos,
            "entrada": registro.timestamp_entrada,
            "saida": timestamp
        }
    
    async def _processar_vaga_status(self, mensagem: Dict[str, Any]) -> Dict[str, Any]:
        """Processa atualização de status de vagas"""
        andar = self._andar_numero_para_nome(mensagem.get('andar'))
        vagas_livres = mensagem.get('vagas_livres', {})
        
        # Atualiza vagas livres
        if andar in self.vagas_livres:
            self.vagas_livres[andar].update(vagas_livres)
            logger.debug(f"Vagas atualizadas para {andar}: {vagas_livres}")
        
        # Atualiza placar
        await self._atualizar_placar()
        
        return {"status": "ok"}
    
    async def _calcular_valor(self, mensagem: Dict[str, Any]) -> Dict[str, Any]:
        """Calcula valor a pagar"""
        placa = mensagem.get('placa')
        
        if placa not in self.veiculos_ativos:
            return {
                "status": "erro",
                "motivo": "veiculo_nao_encontrado",
                "valor": 0.0
            }
        
        registro = self.veiculos_ativos[placa]
        timestamp_atual = datetime.now().isoformat()
        
        valor, tempo_minutos = self._calcular_cobranca(
            registro.timestamp_entrada, timestamp_atual
        )
        
        return {
            "status": "ok",
            "placa": placa,
            "valor": valor,
            "tempo_minutos": tempo_minutos,
            "entrada": registro.timestamp_entrada
        }
    
    async def _processar_passagem_andar(self, mensagem: Dict[str, Any]) -> Dict[str, Any]:
        """Processa passagem entre andares"""
        direcao = mensagem.get('direcao')
        placa = mensagem.get('placa')
        
        # Se tiver placa, atualiza andar do veículo
        if placa and placa in self.veiculos_ativos:
            if direcao == "subindo":
                andar_atual = self.veiculos_ativos[placa].andar_atual
                if andar_atual == 'andar1':
                    self.veiculos_ativos[placa].andar_atual = 'andar2'
                    self.num_carros['andar1'] -= 1
                    self.num_carros['andar2'] += 1
            elif direcao == "descendo":
                andar_atual = self.veiculos_ativos[placa].andar_atual
                if andar_atual == 'andar2':
                    self.veiculos_ativos[placa].andar_atual = 'andar1'
                    self.num_carros['andar2'] -= 1
                    self.num_carros['andar1'] += 1
            
            logger.info(f"Veículo {placa} {direcao}")
        
        return {"status": "ok"}
    
    def _calcular_cobranca(self, timestamp_entrada: str, timestamp_saida: str) -> tuple:
        """Calcula cobrança por tempo de permanência"""
        try:
            entrada = datetime.fromisoformat(timestamp_entrada)
            saida = datetime.fromisoformat(timestamp_saida)
            
            # Calcula tempo em minutos (arredonda para cima)
            tempo_segundos = (saida - entrada).total_seconds()
            tempo_minutos = int(tempo_segundos / 60) + (1 if tempo_segundos % 60 > 0 else 0)
            
            # Calcula valor
            valor = tempo_minutos * self.preco_por_minuto
            
            return valor, tempo_minutos
            
        except Exception as e:
            logger.error(f"Erro ao calcular cobrança: {e}")
            return 0.0, 0
    
    def _esta_lotado(self) -> bool:
        """Verifica se o estacionamento está lotado"""
        for andar in self.vagas_livres.values():
            if sum(andar.values()) > 0:
                return False
        return True
    
    def _andar_numero_para_nome(self, numero: int) -> str:
        """Converte número do andar para nome"""
        mapa = {0: 'terreo', 1: 'andar1', 2: 'andar2'}
        return mapa.get(numero, 'terreo')
    
    async def _atualizar_placar(self):
        """Envia atualização para o placar via servidor térreo"""
        if not self.cliente_terreo:
            return
        
        # Calcula flags
        lotado_geral = self._esta_lotado()
        lotado_andar1 = sum(self.vagas_livres['andar1'].values()) == 0
        lotado_andar2 = sum(self.vagas_livres['andar2'].values()) == 0
        
        mensagem = Mensagem.criar_atualizar_placar({
            'vagas_livres': self.vagas_livres,
            'num_carros': self.num_carros,
            'lotado_geral': lotado_geral,
            'lotado_andar1': lotado_andar1 or self.andar1_bloqueado,
            'lotado_andar2': lotado_andar2 or self.andar2_bloqueado
        })
        
        await self.cliente_terreo.enviar(mensagem)
    
    async def fechar_estacionamento(self, fechar: bool):
        """Fecha ou abre o estacionamento"""
        self.estacionamento_fechado = fechar
        status = "fechado" if fechar else "aberto"
        logger.info(f"Estacionamento {status}")
        await self._atualizar_placar()
    
    async def bloquear_andar(self, andar: int, bloquear: bool):
        """Bloqueia ou desbloqueia um andar"""
        if andar == 1:
            self.andar1_bloqueado = bloquear
        elif andar == 2:
            self.andar2_bloqueado = bloquear
        
        status = "bloqueado" if bloquear else "desbloqueado"
        logger.info(f"Andar {andar} {status}")
        await self._atualizar_placar()
    
    def obter_status(self) -> Dict[str, Any]:
        """Retorna status completo do sistema"""
        return {
            'vagas_livres': self.vagas_livres,
            'vagas_totais': self.vagas_totais,
            'num_carros': self.num_carros,
            'veiculos_ativos': len(self.veiculos_ativos),
            'estacionamento_fechado': self.estacionamento_fechado,
            'andar1_bloqueado': self.andar1_bloqueado,
            'andar2_bloqueado': self.andar2_bloqueado,
            'lotado': self._esta_lotado()
        }
    
    async def iniciar(self):
        """Inicia o servidor central"""
        logger.info("Iniciando Servidor Central...")
        
        # Configura cliente para servidor térreo
        host_terreo = self.config.get('servidor_terreo_host', 'localhost')
        port_terreo = self.config.get('servidor_terreo_port', 5001)
        self.cliente_terreo = ClienteTCP(host_terreo, port_terreo)
        
        # Aguarda um pouco para servidores distribuídos iniciarem
        await asyncio.sleep(2)
        
        # Inicia servidor TCP
        host = self.config.get('servidor_central_host', '0.0.0.0')
        port = self.config.get('servidor_central_port', 5000)
        self.servidor_tcp = ServidorTCP(host, port, self.processar_mensagem)
        
        logger.info(f"Servidor Central iniciado em {host}:{port}")
        
        # Inicia servidor
        await self.servidor_tcp.iniciar()

async def main():
    """Função principal"""
    # Carrega configuração
    config = Config()
    
    # Cria e inicia servidor
    servidor = ServidorCentral(config)
    
    try:
        await servidor.iniciar()
    except KeyboardInterrupt:
        logger.info("Encerrando Servidor Central...")
    except Exception as e:
        logger.error(f"Erro: {e}")

if __name__ == "__main__":
    asyncio.run(main())

