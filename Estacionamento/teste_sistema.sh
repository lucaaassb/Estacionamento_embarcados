#!/bin/bash

# Script de teste para o sistema de estacionamento
# Execute este script para testar o sistema completo

echo "=== SISTEMA DE CONTROLE DE ESTACIONAMENTO ==="
echo "Iniciando testes do sistema..."

# Compilar o projeto
echo "Compilando projeto..."
make clean
make all

if [ $? -ne 0 ]; then
    echo "Erro na compilação!"
    exit 1
fi

echo "Compilação concluída com sucesso!"

# Testar JSON
echo "Testando funcionalidades JSON..."
gcc -o teste_json teste_json.c src/json_utils.c src/common_utils.c -I./inc
./teste_json

if [ $? -eq 0 ]; then
    echo "✅ Teste JSON passou!"
else
    echo "❌ Teste JSON falhou!"
fi

# Verificar dependências
echo "Verificando dependências..."
echo "BCM2835: $(pkg-config --libs bcm2835 2>/dev/null || echo 'Não encontrado')"
echo "MODBUS: $(pkg-config --libs libmodbus 2>/dev/null || echo 'Não encontrado')"

echo ""
echo "=== INSTRUÇÕES DE EXECUÇÃO ==="
echo "1. Servidor Central (rasp40):"
echo "   ssh lucasbarros@164.41.98.2 -p 15000"
echo "   cd ~/estacionamento && ./bin/main c"
echo ""
echo "2. Servidor Térreo (rasp43):"
echo "   ssh lucasbarros@164.41.98.15 -p 13508"
echo "   cd ~/estacionamento && ./bin/main t"
echo ""
echo "3. Servidor 1º Andar (rasp41):"
echo "   ssh lucasbarros@164.41.98.2 -p 15001"
echo "   cd ~/estacionamento && ./bin/main u"
echo ""
echo "4. Servidor 2º Andar (rasp42):"
echo "   ssh lucasbarros@164.41.98.2 -p 15002"
echo "   cd ~/estacionamento && ./bin/main d"
echo ""
echo "=== ORDEM DE EXECUÇÃO ==="
echo "1. Primeiro: Servidor Central"
echo "2. Segundo: Servidor Térreo"
echo "3. Terceiro: Servidor 1º Andar"
echo "4. Quarto: Servidor 2º Andar"
echo ""
echo "Sistema pronto para execução!"
