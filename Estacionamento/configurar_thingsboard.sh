#!/bin/bash

# Script para configurar tokens do ThingsBoard
# Execute este script para configurar os tokens corretos para cada dispositivo

echo "🔧 Configuração de Tokens do ThingsBoard"
echo "========================================"
echo

# Tokens para cada Raspberry Pi (substitua pelos tokens reais do seu projeto)
echo "📋 Configure os tokens para cada dispositivo:"
echo

# rasp46 corresponde ao dashboard: ebaeba20-9e59-11f0-a4ce-1d78bb2310d8
echo "Para o dashboard rasp46 (URL que você mencionou):"
echo "https://tb.fse.lappis.rocks/dashboard/ebaeba20-9e59-11f0-a4ce-1d78bb2310d8"
echo

# Solicitar tokens ao usuário
read -p "🔑 Token do dispositivo Térreo (rasp46): " TOKEN_TERREO
read -p "🔑 Token do dispositivo 1º Andar: " TOKEN_ANDAR1  
read -p "🔑 Token do dispositivo 2º Andar: " TOKEN_ANDAR2

# Atualizar arquivo de header
echo "📝 Atualizando tokens no código..."

# Backup do arquivo original
cp inc/thingsboard.h inc/thingsboard.h.backup

# Substituir tokens no arquivo
sed -i "s/YOUR_TERREO_TOKEN/$TOKEN_TERREO/g" inc/thingsboard.h
sed -i "s/YOUR_ANDAR1_TOKEN/$TOKEN_ANDAR1/g" inc/thingsboard.h  
sed -i "s/YOUR_ANDAR2_TOKEN/$TOKEN_ANDAR2/g" inc/thingsboard.h

echo "✅ Tokens configurados com sucesso!"
echo

# Salvar tokens em arquivo de configuração
cat > thingsboard_tokens.conf << EOF
# Configuração de Tokens ThingsBoard
# Gerado automaticamente em $(date)

# Tokens dos dispositivos
TB_TOKEN_TERREO=$TOKEN_TERREO
TB_TOKEN_ANDAR1=$TOKEN_ANDAR1
TB_TOKEN_ANDAR2=$TOKEN_ANDAR2

# URLs dos Dashboards
DASHBOARD_TERREO=https://tb.fse.lappis.rocks/dashboard/ebaeba20-9e59-11f0-a4ce-1d78bb2310d8?publicId=86d17ff0-e010-11ef-9ab8-4774ff1517e8
DASHBOARD_ANDAR1=https://tb.fse.lappis.rocks/dashboard/362971f0-9e30-11f0-a4ce-1d78bb2310d8?publicId=86d17ff0-e010-11ef-9ab8-4774ff1517e8
DASHBOARD_ANDAR2=https://tb.fse.lappis.rocks/dashboard/a926da80-9e30-11f0-a4ce-1d78bb2310d8?publicId=86d17ff0-e010-11ef-9ab8-4774ff1517e8

# Servidor ThingsBoard
TB_SERVER=tb.fse.lappis.rocks
TB_PORT=443
TB_PROTOCOL=https
EOF

echo "💾 Configuração salva em: thingsboard_tokens.conf"
echo

echo "🔄 Recompile o sistema para aplicar as mudanças:"
echo "   make clean && make all"
echo

echo "📊 Após executar o sistema, os dados aparecerão no dashboard:"
echo "   $DASHBOARD_TERREO"
echo

echo "📋 Dados que serão enviados:"
echo "   • Estado das vagas (ocupada/livre)"
echo "   • Eventos de entrada de veículos" 
echo "   • Eventos de saída de veículos"
echo "   • Valores de cobrança"
echo "   • Contadores totais"
echo "   • Dados dos sensores em tempo real"
echo

echo "✅ Configuração concluída!"
