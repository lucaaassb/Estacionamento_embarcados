#!/usr/bin/env python3
"""
Script auxiliar para processar arquivo config.env
Pode gerar header C ou apenas validar as configurações
"""

import sys
import os
from pathlib import Path

def parse_env_file(filepath):
    """Parse arquivo .env e retorna dicionário com configurações"""
    config = {}
    
    if not os.path.exists(filepath):
        print(f"Erro: Arquivo '{filepath}' não encontrado!", file=sys.stderr)
        return None
    
    with open(filepath, 'r', encoding='utf-8') as f:
        for line_num, line in enumerate(f, 1):
            # Remove espaços e pula linhas vazias ou comentários
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            
            # Parse KEY=VALUE
            if '=' in line:
                key, value = line.split('=', 1)
                key = key.strip()
                value = value.strip()
                
                # Remove aspas se existirem
                if value.startswith('"') and value.endswith('"'):
                    value = value[1:-1]
                elif value.startswith("'") and value.endswith("'"):
                    value = value[1:-1]
                
                config[key] = value
            else:
                print(f"Aviso: Linha {line_num} mal formatada: {line}", file=sys.stderr)
    
    return config

def convert_value_to_c(key, value):
    """Converte valor para formato C adequado"""
    # Valores hexadecimais
    if value.startswith('0x') or value.startswith('0X'):
        return value
    
    # Strings (caminhos, etc)
    if key.endswith('_FILE') or key.endswith('_DIR') or key.endswith('_PORT') or 'PATH' in key:
        return f'"{value}"'
    
    # Booleanos
    if value.lower() in ['true', 'false']:
        return value.lower()
    
    # Números decimais
    try:
        if '.' in value:
            float(value)
            return value + 'f'  # float literal
        else:
            int(value)
            return value
    except ValueError:
        pass
    
    # Default: string
    return f'"{value}"'

def generate_c_header(config):
    """Gera header C a partir das configurações"""
    header = """/*
 * config_generated.h
 * Arquivo gerado automaticamente a partir de config.env
 * NÃO EDITE ESTE ARQUIVO MANUALMENTE!
 * 
 * Para regenerar, execute:
 *   python3 generate_config.py config.env > inc/config_generated.h
 */

#ifndef CONFIG_GENERATED_H
#define CONFIG_GENERATED_H

/* ============================================================================
 * CONFIGURAÇÕES DO SISTEMA DE ESTACIONAMENTO
 * ============================================================================ */

"""
    
    # Agrupa configurações por prefixo
    sections = {}
    for key, value in sorted(config.items()):
        prefix = key.split('_')[0]
        if prefix not in sections:
            sections[prefix] = []
        sections[prefix].append((key, value))
    
    # Gera defines para cada seção
    for section, items in sorted(sections.items()):
        header += f"/* {section.upper()} */\n"
        for key, value in items:
            c_value = convert_value_to_c(key, value)
            header += f"#define {key:40s} {c_value}\n"
        header += "\n"
    
    header += "#endif /* CONFIG_GENERATED_H */\n"
    
    return header

def validate_config(config):
    """Valida configurações obrigatórias"""
    required_keys = [
        'MODBUS_SERIAL_PORT',
        'MODBUS_SERIAL_BAUD',
        'MODBUS_ADDR_LPR_ENTRADA',
        'MODBUS_ADDR_LPR_SAIDA',
        'MODBUS_ADDR_PLACAR',
    ]
    
    errors = []
    warnings = []
    
    # Verifica chaves obrigatórias
    for key in required_keys:
        if key not in config:
            errors.append(f"Configuração obrigatória ausente: {key}")
    
    # Valida valores
    if 'MODBUS_SERIAL_BAUD' in config:
        try:
            baud = int(config['MODBUS_SERIAL_BAUD'])
            valid_bauds = [9600, 19200, 38400, 57600, 115200, 230400]
            if baud not in valid_bauds:
                warnings.append(f"Baudrate {baud} não é padrão. Valores comuns: {valid_bauds}")
        except ValueError:
            errors.append(f"MODBUS_SERIAL_BAUD deve ser um número inteiro")
    
    # Valida porta serial
    if 'MODBUS_SERIAL_PORT' in config:
        port = config['MODBUS_SERIAL_PORT']
        if not port.startswith('/dev/'):
            warnings.append(f"Porta serial '{port}' não parece ser um dispositivo Linux")
    
    # Valida endereços MODBUS (devem ser hexadecimais)
    for key in ['MODBUS_ADDR_LPR_ENTRADA', 'MODBUS_ADDR_LPR_SAIDA', 'MODBUS_ADDR_PLACAR']:
        if key in config:
            addr = config[key]
            if not (addr.startswith('0x') or addr.startswith('0X')):
                warnings.append(f"{key} deveria estar em formato hexadecimal (ex: 0x11)")
    
    # Valida GPIO (não pode haver conflitos de pinos)
    gpio_pins = []
    for key, value in config.items():
        if any(x in key for x in ['GPIO', 'ENDERECO', 'SENSOR', 'MOTOR']) and key.endswith(tuple('0123456789')):
            try:
                pin = int(value)
                if pin in gpio_pins:
                    errors.append(f"Conflito de GPIO: pino {pin} usado em {key} já está em uso")
                gpio_pins.append(pin)
            except ValueError:
                pass
    
    return errors, warnings

def print_config_summary(config):
    """Imprime resumo das configurações"""
    print("=" * 70)
    print("RESUMO DAS CONFIGURAÇÕES DO SISTEMA DE ESTACIONAMENTO")
    print("=" * 70)
    print()
    
    # Comunicação MODBUS
    print("📡 COMUNICAÇÃO MODBUS")
    print(f"  Porta Serial:        {config.get('MODBUS_SERIAL_PORT', 'NÃO DEFINIDO')}")
    print(f"  Baudrate:            {config.get('MODBUS_SERIAL_BAUD', 'NÃO DEFINIDO')} bps")
    print(f"  Timeout:             {config.get('MODBUS_TIMEOUT_MS', 'NÃO DEFINIDO')} ms")
    print(f"  Retries:             {config.get('MODBUS_RETRIES', 'NÃO DEFINIDO')}")
    print()
    
    # Dispositivos
    print("🎯 ENDEREÇOS DOS DISPOSITIVOS")
    print(f"  LPR Entrada:         {config.get('MODBUS_ADDR_LPR_ENTRADA', 'NÃO DEFINIDO')}")
    print(f"  LPR Saída:           {config.get('MODBUS_ADDR_LPR_SAIDA', 'NÃO DEFINIDO')}")
    print(f"  Placar:              {config.get('MODBUS_ADDR_PLACAR', 'NÃO DEFINIDO')}")
    print()
    
    # Matrícula
    print("🎓 MATRÍCULA")
    mat = [
        config.get('MODBUS_MATRICULA_BYTE1', '??'),
        config.get('MODBUS_MATRICULA_BYTE2', '??'),
        config.get('MODBUS_MATRICULA_BYTE3', '??'),
        config.get('MODBUS_MATRICULA_BYTE4', '??'),
    ]
    print(f"  Matrícula:           {' '.join(mat)}")
    print()
    
    # Rede
    print("🌐 REDE TCP/IP")
    print(f"  Servidor Host:       {config.get('CENTRAL_SERVER_HOST', 'NÃO DEFINIDO')}")
    print(f"  Servidor Port:       {config.get('CENTRAL_SERVER_PORT', 'NÃO DEFINIDO')}")
    print()
    
    # Cobrança
    print("💰 COBRANÇA")
    print(f"  Preço/minuto:        R$ {config.get('PRECO_POR_MINUTO', 'NÃO DEFINIDO')}")
    print()
    
    # Câmeras
    print("📷 CÂMERAS LPR")
    print(f"  Confiança mínima:    {config.get('LPR_CONFIANCA_MINIMA', 'NÃO DEFINIDO')}%")
    print(f"  Confiança baixa:     {config.get('LPR_CONFIANCA_BAIXA', 'NÃO DEFINIDO')}%")
    print(f"  Modo degradado:      {config.get('LPR_MODO_DEGRADADO', 'NÃO DEFINIDO')}")
    print()
    
    # Vagas
    total_vagas = 0
    for andar in ['TERREO', 'ANDAR1', 'ANDAR2']:
        num_key = f'{andar}_NUM_VAGAS'
        if num_key in config:
            try:
                total_vagas += int(config[num_key])
            except ValueError:
                pass
    
    print("🅿️ VAGAS")
    print(f"  Térreo:              {config.get('TERREO_NUM_VAGAS', 'NÃO DEFINIDO')} vagas")
    print(f"  1º Andar:            {config.get('ANDAR1_NUM_VAGAS', 'NÃO DEFINIDO')} vagas")
    print(f"  2º Andar:            {config.get('ANDAR2_NUM_VAGAS', 'NÃO DEFINIDO')} vagas")
    print(f"  TOTAL:               {total_vagas} vagas")
    print()
    
    print("=" * 70)

def main():
    if len(sys.argv) < 2:
        print(f"Uso: {sys.argv[0]} <config.env> [--header|--validate|--summary]")
        print()
        print("Opções:")
        print("  --header     Gera header C para inclusão no código")
        print("  --validate   Valida configurações")
        print("  --summary    Mostra resumo das configurações (padrão)")
        print()
        print("Exemplos:")
        print(f"  {sys.argv[0]} config.env --summary")
        print(f"  {sys.argv[0]} config.env --header > inc/config_generated.h")
        print(f"  {sys.argv[0]} config.env --validate")
        sys.exit(1)
    
    env_file = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else '--summary'
    
    # Parse arquivo
    config = parse_env_file(env_file)
    if config is None:
        sys.exit(1)
    
    # Executa ação solicitada
    if mode == '--header':
        header = generate_c_header(config)
        print(header)
    
    elif mode == '--validate':
        errors, warnings = validate_config(config)
        
        if warnings:
            print("⚠️  AVISOS:", file=sys.stderr)
            for warning in warnings:
                print(f"  - {warning}", file=sys.stderr)
            print(file=sys.stderr)
        
        if errors:
            print("❌ ERROS:", file=sys.stderr)
            for error in errors:
                print(f"  - {error}", file=sys.stderr)
            sys.exit(1)
        else:
            print("✅ Configurações válidas!")
    
    elif mode == '--summary':
        print_config_summary(config)
        print()
        errors, warnings = validate_config(config)
        if errors or warnings:
            print("⚠️  Execute com --validate para ver detalhes de validação")
    
    else:
        print(f"Modo desconhecido: {mode}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()

