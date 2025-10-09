#!/bin/bash

# Script de inicialização de todos os servidores usando TMUX
# Este script cria uma sessão tmux com 4 painéis, um para cada servidor

echo "=========================================="
echo "  Sistema de Estacionamento Distribuído  "
echo "=========================================="
echo ""
echo "Iniciando todos os servidores com TMUX..."
echo ""

# Verifica se tmux está instalado
if ! command -v tmux &> /dev/null; then
    echo "❌ Erro: tmux não está instalado!"
    echo ""
    echo "Instale com:"
    echo "  sudo apt-get install tmux"
    echo ""
    exit 1
fi

# Verifica se a sessão já existe
if tmux has-session -t estacionamento 2>/dev/null; then
    echo "⚠️  Sessão 'estacionamento' já existe!"
    echo ""
    echo "Opções:"
    echo "  1. Reconectar à sessão existente:"
    echo "     tmux attach-session -t estacionamento"
    echo ""
    echo "  2. Encerrar a sessão existente e criar nova:"
    echo "     tmux kill-session -t estacionamento"
    echo "     ./start_all_servers.sh"
    echo ""
    exit 1
fi

# Obter diretório do script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "📂 Diretório do projeto: $SCRIPT_DIR"
echo ""

# Criar nova sessão tmux em background
echo "🚀 Criando sessão tmux 'estacionamento'..."
tmux new-session -d -s estacionamento -c "$SCRIPT_DIR"

# Renomear primeira janela
tmux rename-window -t estacionamento:0 'Servidores'

# Criar layout com 4 painéis (grade 2x2)
echo "📐 Configurando layout com 4 painéis..."
tmux split-window -h -t estacionamento:0
tmux split-window -v -t estacionamento:0.0
tmux split-window -v -t estacionamento:0.2

# Ajustar layout para ficar balanceado
tmux select-layout -t estacionamento:0 tiled

# Executar Servidor Central no painel 0 (superior esquerdo)
echo "🖥️  Iniciando Servidor Central (painel 0)..."
tmux send-keys -t estacionamento:0.0 "cd '$SCRIPT_DIR'" C-m
tmux send-keys -t estacionamento:0.0 "clear" C-m
tmux send-keys -t estacionamento:0.0 "echo '=== SERVIDOR CENTRAL ==='" C-m
tmux send-keys -t estacionamento:0.0 "echo 'Iniciando em 1 segundo...'" C-m
tmux send-keys -t estacionamento:0.0 "sleep 1" C-m
tmux send-keys -t estacionamento:0.0 "./run_servidor_central.sh" C-m

# Executar Servidor Térreo no painel 1 (superior direito)
echo "🏢 Iniciando Servidor Térreo (painel 1)..."
tmux send-keys -t estacionamento:0.1 "cd '$SCRIPT_DIR'" C-m
tmux send-keys -t estacionamento:0.1 "clear" C-m
tmux send-keys -t estacionamento:0.1 "echo '=== SERVIDOR TÉRREO ==='" C-m
tmux send-keys -t estacionamento:0.1 "echo 'Aguardando Central (3 segundos)...'" C-m
tmux send-keys -t estacionamento:0.1 "sleep 3" C-m
tmux send-keys -t estacionamento:0.1 "./run_servidor_terreo.sh" C-m

# Executar Servidor 1º Andar no painel 2 (inferior esquerdo)
echo "🏢 Iniciando Servidor 1º Andar (painel 2)..."
tmux send-keys -t estacionamento:0.2 "cd '$SCRIPT_DIR'" C-m
tmux send-keys -t estacionamento:0.2 "clear" C-m
tmux send-keys -t estacionamento:0.2 "echo '=== SERVIDOR 1º ANDAR ==='" C-m
tmux send-keys -t estacionamento:0.2 "echo 'Aguardando Central (3 segundos)...'" C-m
tmux send-keys -t estacionamento:0.2 "sleep 3" C-m
tmux send-keys -t estacionamento:0.2 "./run_servidor_andar1.sh" C-m

# Executar Servidor 2º Andar no painel 3 (inferior direito)
echo "🏢 Iniciando Servidor 2º Andar (painel 3)..."
tmux send-keys -t estacionamento:0.3 "cd '$SCRIPT_DIR'" C-m
tmux send-keys -t estacionamento:0.3 "clear" C-m
tmux send-keys -t estacionamento:0.3 "echo '=== SERVIDOR 2º ANDAR ==='" C-m
tmux send-keys -t estacionamento:0.3 "echo 'Aguardando Central (3 segundos)...'" C-m
tmux send-keys -t estacionamento:0.3 "sleep 3" C-m
tmux send-keys -t estacionamento:0.3 "./run_servidor_andar2.sh" C-m

# Selecionar painel do Servidor Central (onde está a interface)
tmux select-pane -t estacionamento:0.0

echo ""
echo "✅ Todos os servidores foram iniciados!"
echo ""
echo "=========================================="
echo "            COMO USAR"
echo "=========================================="
echo ""
echo "📺 Para acessar a interface:"
echo "   tmux attach-session -t estacionamento"
echo ""
echo "🎮 Comandos dentro do tmux:"
echo "   Ctrl+B + ↑/↓/←/→  - Navegar entre painéis"
echo "   Ctrl+B + [        - Modo de rolagem (q para sair)"
echo "   Ctrl+B + d        - Desconectar (servidores continuam rodando)"
echo "   Ctrl+B + z        - Maximizar/restaurar painel atual"
echo ""
echo "🛑 Para encerrar tudo:"
echo "   Ctrl+B + :kill-session"
echo "   ou"
echo "   tmux kill-session -t estacionamento"
echo ""
echo "🔄 Para reconectar após desconectar:"
echo "   tmux attach-session -t estacionamento"
echo ""
echo "=========================================="
echo ""
echo "⏳ Aguarde alguns segundos para os servidores iniciarem completamente..."
echo ""

