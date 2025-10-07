#!/bin/bash

# Script para verificar se os pinos GPIO estão corretamente definidos
# conforme as tabelas do README

echo "🔍 Verificando mapeamento de pinos GPIO..."
echo

# Função para verificar arquivo C
verificar_gpio() {
    local arquivo=$1
    local andar=$2
    echo "📋 Verificando $andar ($arquivo):"
    
    if [ ! -f "$arquivo" ]; then
        echo "❌ Arquivo não encontrado: $arquivo"
        return 1
    fi
    
    case $andar in
        "Térreo")
            echo "   ENDERECO_01 (GPIO17): $(grep -o 'RPI_V2_GPIO_P1_[0-9]*' $arquivo | grep -A1 ENDERECO_01 | tail -1 || echo 'NÃO ENCONTRADO')"
            echo "   ENDERECO_02 (GPIO18): $(grep -o 'RPI_V2_GPIO_P1_[0-9]*' $arquivo | grep -A1 ENDERECO_02 | tail -1 || echo 'NÃO ENCONTRADO')"
            echo "   SENSOR_DE_VAGA (GPIO8): $(grep -A1 'SENSOR_DE_VAGA' $arquivo | grep -o 'RPI_V2_GPIO_P1_[0-9]*' || echo 'NÃO ENCONTRADO')"
            ;;
        "1º Andar")
            echo "   ENDERECO_01 (GPIO16): $(grep -A1 'ENDERECO_01' $arquivo | grep -o 'RPI_V2_GPIO_P1_[0-9]*' || echo 'NÃO ENCONTRADO')"
            echo "   ENDERECO_02 (GPIO20): $(grep -A1 'ENDERECO_02' $arquivo | grep -o 'RPI_V2_GPIO_P1_[0-9]*' || echo 'NÃO ENCONTRADO')"
            echo "   ENDERECO_03 (GPIO21): $(grep -A1 'ENDERECO_03' $arquivo | grep -o 'RPI_V2_GPIO_P1_[0-9]*' || echo 'NÃO ENCONTRADO')"
            echo "   SENSOR_DE_VAGA (GPIO27): $(grep -A1 'SENSOR_DE_VAGA' $arquivo | grep -o 'RPI_V2_GPIO_P1_[0-9]*' || echo 'NÃO ENCONTRADO')"
            ;;
        "2º Andar")
            echo "   ENDERECO_01 (GPIO0): $(grep -A1 'ENDERECO_01' $arquivo | grep -o 'RPI_V2_GPIO_P1_[0-9]*' || echo 'NÃO ENCONTRADO')"
            echo "   ENDERECO_02 (GPIO5): $(grep -A1 'ENDERECO_02' $arquivo | grep -o 'RPI_V2_GPIO_P1_[0-9]*' || echo 'NÃO ENCONTRADO')"
            echo "   ENDERECO_03 (GPIO6): $(grep -A1 'ENDERECO_03' $arquivo | grep -o 'RPI_V2_GPIO_P1_[0-9]*' || echo 'NÃO ENCONTRADO')"
            echo "   SENSOR_DE_VAGA (GPIO13): $(grep -A1 'SENSOR_DE_VAGA' $arquivo | grep -o 'RPI_V2_GPIO_P1_[0-9]*' || echo 'NÃO ENCONTRADO')"
            ;;
    esac
    echo
}

# Verificar cada arquivo
verificar_gpio "src/terreo.c" "Térreo"
verificar_gpio "src/1Andar.c" "1º Andar"
verificar_gpio "src/2Andar.c" "2º Andar"

echo "✅ Verificação de GPIO concluída!"
echo "📖 Referência - README Tabelas 1, 2 e 3"
