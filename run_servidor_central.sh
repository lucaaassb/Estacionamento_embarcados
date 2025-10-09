#!/bin/bash

# Script para iniciar o Servidor Central com Interface

echo "Iniciando Servidor Central do Sistema de Estacionamento..."
echo ""

cd "$(dirname "$0")"

# Verifica se o Python está instalado
if ! command -v python3 &> /dev/null; then
    echo "Erro: Python 3 não encontrado!"
    exit 1
fi

# Adiciona o diretório atual ao PYTHONPATH
export PYTHONPATH="${PYTHONPATH}:$(pwd)"

# Inicia servidor com interface
python3 -m servidor_central.interface

