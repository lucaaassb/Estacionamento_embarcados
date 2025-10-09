"""
Módulo de Configuração
Carrega e gerencia as configurações do sistema
"""

import os
from typing import Dict, Any

class Config:
    """Classe para gerenciar configurações do sistema"""
    
    def __init__(self, config_file: str = None):
        self.config = self._load_config(config_file)
    
    def _load_config(self, config_file: str = None) -> Dict[str, Any]:
        """Carrega configurações de arquivo ou usa valores padrão"""
        config = {
            # Servidor Central
            'servidor_central_host': '192.168.0.100',
            'servidor_central_port': 5000,
            
            # Servidor Térreo
            'servidor_terreo_host': '0.0.0.0',
            'servidor_terreo_port': 5001,
            
            # Servidor 1º Andar
            'servidor_andar1_host': '0.0.0.0',
            'servidor_andar1_port': 5002,
            
            # Servidor 2º Andar
            'servidor_andar2_host': '0.0.0.0',
            'servidor_andar2_port': 5003,
            
            # MODBUS
            'modbus_port': '/dev/ttyUSB0',
            'modbus_baudrate': 115200,
            'modbus_timeout': 0.5,
            'modbus_retries': 3,
            
            # Endereços MODBUS
            'modbus_camera_entrada': 0x11,
            'modbus_camera_saida': 0x12,
            'modbus_placar': 0x20,
            
            # Matrícula
            'matricula': '0000',
            
            # Cobrança
            'preco_por_minuto': 0.15,
            
            # Vagas
            'vagas_terreo_pne': 2,
            'vagas_terreo_idoso': 2,
            'vagas_terreo_comuns': 4,
            'vagas_andar1_pne': 2,
            'vagas_andar1_idoso': 2,
            'vagas_andar1_comuns': 4,
            'vagas_andar2_pne': 2,
            'vagas_andar2_idoso': 2,
            'vagas_andar2_comuns': 4,
            
            # Confiança
            'confianca_minima': 70,
        }
        
        # Carrega de arquivo se fornecido
        if config_file and os.path.exists(config_file):
            with open(config_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith('#'):
                        if '=' in line:
                            key, value = line.split('=', 1)
                            key = key.strip().lower()
                            value = value.strip()
                            # Tenta converter para tipos apropriados
                            if value.isdigit():
                                value = int(value)
                            elif value.replace('.', '', 1).isdigit():
                                value = float(value)
                            config[key] = value
        
        return config
    
    def get(self, key: str, default: Any = None) -> Any:
        """Obtém valor de configuração"""
        return self.config.get(key, default)
    
    def set(self, key: str, value: Any):
        """Define valor de configuração"""
        self.config[key] = value

