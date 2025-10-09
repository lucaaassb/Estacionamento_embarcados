"""
Módulo de Mensagens
Define os tipos de mensagens trocadas entre servidores via TCP/IP
"""

import json
from datetime import datetime
from typing import Dict, Any, Optional

class TipoMensagem:
    """Tipos de mensagens do sistema"""
    ENTRADA_OK = "entrada_ok"
    SAIDA_OK = "saida_ok"
    VAGA_STATUS = "vaga_status"
    PASSAGEM_ANDAR = "passagem_andar"
    CALCULAR_VALOR = "calcular_valor"
    RESPOSTA_VALOR = "resposta_valor"
    COMANDO_CANCELA = "comando_cancela"
    ATUALIZAR_PLACAR = "atualizar_placar"
    FECHAR_ESTACIONAMENTO = "fechar_estacionamento"
    BLOQUEAR_ANDAR = "bloquear_andar"
    STATUS_SISTEMA = "status_sistema"
    HEARTBEAT = "heartbeat"

class Mensagem:
    """Classe base para mensagens do sistema"""
    
    @staticmethod
    def criar_entrada_ok(placa: str, confianca: int, andar: int) -> Dict[str, Any]:
        """Cria mensagem de entrada confirmada"""
        return {
            "tipo": TipoMensagem.ENTRADA_OK,
            "placa": placa,
            "conf": confianca,
            "ts": datetime.now().isoformat(),
            "andar": andar
        }
    
    @staticmethod
    def criar_saida_ok(placa: str, confianca: int) -> Dict[str, Any]:
        """Cria mensagem de saída confirmada"""
        return {
            "tipo": TipoMensagem.SAIDA_OK,
            "placa": placa,
            "conf": confianca,
            "ts": datetime.now().isoformat()
        }
    
    @staticmethod
    def criar_vaga_status(andar: int, vagas_livres: Dict[str, int]) -> Dict[str, Any]:
        """Cria mensagem de status de vagas"""
        return {
            "tipo": TipoMensagem.VAGA_STATUS,
            "andar": andar,
            "vagas_livres": vagas_livres,
            "ts": datetime.now().isoformat()
        }
    
    @staticmethod
    def criar_passagem_andar(direcao: str, placa: Optional[str] = None) -> Dict[str, Any]:
        """Cria mensagem de passagem entre andares"""
        return {
            "tipo": TipoMensagem.PASSAGEM_ANDAR,
            "direcao": direcao,  # "subindo" ou "descendo"
            "placa": placa,
            "ts": datetime.now().isoformat()
        }
    
    @staticmethod
    def criar_calcular_valor(placa: str) -> Dict[str, Any]:
        """Cria mensagem de solicitação de cálculo de valor"""
        return {
            "tipo": TipoMensagem.CALCULAR_VALOR,
            "placa": placa,
            "ts": datetime.now().isoformat()
        }
    
    @staticmethod
    def criar_resposta_valor(placa: str, valor: float, tempo_minutos: int, 
                            entrada: str, saida: str) -> Dict[str, Any]:
        """Cria mensagem de resposta com valor calculado"""
        return {
            "tipo": TipoMensagem.RESPOSTA_VALOR,
            "placa": placa,
            "valor": valor,
            "tempo_minutos": tempo_minutos,
            "entrada": entrada,
            "saida": saida,
            "ts": datetime.now().isoformat()
        }
    
    @staticmethod
    def criar_comando_cancela(cancela: str, acao: str) -> Dict[str, Any]:
        """Cria comando para cancela"""
        return {
            "tipo": TipoMensagem.COMANDO_CANCELA,
            "cancela": cancela,  # "entrada" ou "saida"
            "acao": acao,  # "abrir" ou "fechar"
            "ts": datetime.now().isoformat()
        }
    
    @staticmethod
    def criar_atualizar_placar(dados_vagas: Dict[str, int]) -> Dict[str, Any]:
        """Cria mensagem para atualizar placar"""
        return {
            "tipo": TipoMensagem.ATUALIZAR_PLACAR,
            "dados": dados_vagas,
            "ts": datetime.now().isoformat()
        }
    
    @staticmethod
    def criar_fechar_estacionamento(fechar: bool) -> Dict[str, Any]:
        """Cria comando para fechar/abrir estacionamento"""
        return {
            "tipo": TipoMensagem.FECHAR_ESTACIONAMENTO,
            "fechar": fechar,
            "ts": datetime.now().isoformat()
        }
    
    @staticmethod
    def criar_bloquear_andar(andar: int, bloquear: bool) -> Dict[str, Any]:
        """Cria comando para bloquear/desbloquear andar"""
        return {
            "tipo": TipoMensagem.BLOQUEAR_ANDAR,
            "andar": andar,
            "bloquear": bloquear,
            "ts": datetime.now().isoformat()
        }
    
    @staticmethod
    def serializar(mensagem: Dict[str, Any]) -> str:
        """Serializa mensagem para JSON"""
        return json.dumps(mensagem)
    
    @staticmethod
    def desserializar(dados: str) -> Dict[str, Any]:
        """Desserializa mensagem de JSON"""
        return json.loads(dados)

