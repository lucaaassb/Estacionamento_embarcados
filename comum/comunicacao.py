"""
Módulo de Comunicação TCP/IP
Gerencia conexões e troca de mensagens entre servidores
"""

import asyncio
import json
import logging
from typing import Callable, Optional, Dict, Any

logger = logging.getLogger(__name__)

class ServidorTCP:
    """Servidor TCP assíncrono para receber mensagens"""
    
    def __init__(self, host: str, port: int, callback: Callable):
        self.host = host
        self.port = port
        self.callback = callback
        self.server = None
        self.running = False
    
    async def iniciar(self):
        """Inicia o servidor TCP"""
        self.server = await asyncio.start_server(
            self._handle_client, self.host, self.port
        )
        self.running = True
        logger.info(f"Servidor TCP iniciado em {self.host}:{self.port}")
        async with self.server:
            await self.server.serve_forever()
    
    async def _handle_client(self, reader: asyncio.StreamReader, 
                            writer: asyncio.StreamWriter):
        """Gerencia conexão com cliente"""
        addr = writer.get_extra_info('peername')
        logger.debug(f"Nova conexão de {addr}")
        
        try:
            while True:
                # Lê tamanho da mensagem (4 bytes)
                size_data = await reader.readexactly(4)
                if not size_data:
                    break
                
                msg_size = int.from_bytes(size_data, byteorder='big')
                
                # Lê mensagem completa
                data = await reader.readexactly(msg_size)
                mensagem = json.loads(data.decode())
                
                logger.debug(f"Mensagem recebida de {addr}: {mensagem['tipo']}")
                
                # Processa mensagem via callback
                resposta = await self.callback(mensagem)
                
                # Envia resposta se houver
                if resposta:
                    resposta_json = json.dumps(resposta).encode()
                    writer.write(len(resposta_json).to_bytes(4, byteorder='big'))
                    writer.write(resposta_json)
                    await writer.drain()
                
        except asyncio.IncompleteReadError:
            logger.debug(f"Conexão fechada por {addr}")
        except Exception as e:
            logger.error(f"Erro ao processar mensagem de {addr}: {e}")
        finally:
            writer.close()
            await writer.wait_closed()
    
    async def parar(self):
        """Para o servidor TCP"""
        if self.server:
            self.server.close()
            await self.server.wait_closed()
            self.running = False
            logger.info("Servidor TCP parado")

class ClienteTCP:
    """Cliente TCP para enviar mensagens"""
    
    def __init__(self, host: str, port: int, timeout: float = 5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self._reader = None
        self._writer = None
        self._connected = False
        self._lock = asyncio.Lock()
    
    async def conectar(self) -> bool:
        """Conecta ao servidor"""
        async with self._lock:
            if self._connected:
                return True
            
            try:
                self._reader, self._writer = await asyncio.wait_for(
                    asyncio.open_connection(self.host, self.port),
                    timeout=self.timeout
                )
                self._connected = True
                logger.info(f"Conectado ao servidor {self.host}:{self.port}")
                return True
            except Exception as e:
                logger.error(f"Erro ao conectar a {self.host}:{self.port}: {e}")
                return False
    
    async def desconectar(self):
        """Desconecta do servidor"""
        async with self._lock:
            if self._writer:
                self._writer.close()
                await self._writer.wait_closed()
            self._connected = False
            logger.info(f"Desconectado de {self.host}:{self.port}")
    
    async def enviar(self, mensagem: Dict[str, Any], 
                    esperar_resposta: bool = False) -> Optional[Dict[str, Any]]:
        """Envia mensagem ao servidor"""
        # Tenta conectar se não estiver conectado
        if not self._connected:
            if not await self.conectar():
                return None
        
        try:
            # Serializa mensagem
            msg_json = json.dumps(mensagem).encode()
            
            # Envia tamanho e mensagem
            self._writer.write(len(msg_json).to_bytes(4, byteorder='big'))
            self._writer.write(msg_json)
            await self._writer.drain()
            
            logger.debug(f"Mensagem enviada para {self.host}:{self.port}: {mensagem['tipo']}")
            
            # Aguarda resposta se solicitado
            if esperar_resposta:
                size_data = await asyncio.wait_for(
                    self._reader.readexactly(4),
                    timeout=self.timeout
                )
                msg_size = int.from_bytes(size_data, byteorder='big')
                
                data = await asyncio.wait_for(
                    self._reader.readexactly(msg_size),
                    timeout=self.timeout
                )
                resposta = json.loads(data.decode())
                logger.debug(f"Resposta recebida: {resposta}")
                return resposta
            
            return {"status": "ok"}
            
        except Exception as e:
            logger.error(f"Erro ao enviar mensagem: {e}")
            self._connected = False
            return None
    
    async def enviar_com_retry(self, mensagem: Dict[str, Any], 
                              tentativas: int = 3,
                              esperar_resposta: bool = False) -> Optional[Dict[str, Any]]:
        """Envia mensagem com tentativas de reconexão"""
        for i in range(tentativas):
            resposta = await self.enviar(mensagem, esperar_resposta)
            if resposta:
                return resposta
            
            if i < tentativas - 1:
                logger.warning(f"Tentativa {i+1} falhou, reconectando...")
                await self.desconectar()
                await asyncio.sleep(1)
        
        logger.error(f"Falha ao enviar mensagem após {tentativas} tentativas")
        return None

