#!/bin/bash

# Script para iniciar o Servidor do Andar Térreo

echo "Iniciando Servidor do Andar Térreo..."
echo ""

cd "$(dirname "$0")"

# Verifica se o Python está instalado
if ! command -v python3 &> /dev/null; then
    echo "Erro: Python 3 não encontrado!"
    exit 1
fi

# Adiciona o diretório atual ao PYTHONPATH
export PYTHONPATH="${PYTHONPATH}:$(pwd)"

# Inicia servidor
python3 -m servidor_terreo.servidor_terreo

