#!/bin/bash

# Script para iniciar o Servidor do 2º Andar

echo "Iniciando Servidor do 2º Andar..."
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
python3 -m servidor_andar.servidor_andar 2

