# 🔧 Instalação e Execução em Raspberry Pi via SSH

Este guia mostra como instalar e executar o sistema em uma Raspberry Pi acessada remotamente via SSH.

---

## 📋 Pré-requisitos

### No seu computador (local)
- Cliente SSH instalado
- Cliente SCP (normalmente vem com SSH)

### Na Raspberry Pi (remota)
- Raspberry Pi OS instalado
- SSH habilitado
- Python 3.7+ instalado
- Acesso à rede (mesmo IP que seu computador ou acessível via rede)

---

## 🚀 Instalação Rápida (Passo a Passo)

### 1️⃣ Copiar Projeto para a Raspberry Pi

No seu computador local, execute:

```bash
# Sintaxe: scp -r [pasta_local] [usuario]@[ip_raspberry]:[destino]
scp -r /home/lucascaldasb/Documentos/FSE-2025-2/Estacionamento_embarcados pi@192.168.0.100:~/

# Exemplo com IP diferente:
# scp -r Estacionamento_embarcados pi@10.0.0.50:~/
```

**Notas**:
- Substitua `pi` pelo seu usuário da Raspberry Pi
- Substitua `192.168.0.100` pelo IP da sua Raspberry Pi
- Será solicitada a senha da Raspberry Pi

**Para descobrir o IP da Raspberry Pi**:
```bash
# Na Raspberry Pi, execute:
hostname -I
# ou
ip addr show
```

---

### 2️⃣ Conectar via SSH

```bash
ssh pi@192.168.0.100
```

Você estará agora dentro da Raspberry Pi via terminal SSH.

---

### 3️⃣ Instalar Dependências na Raspberry Pi

```bash
# Entre no diretório do projeto
cd ~/Estacionamento_embarcados

# Atualize o sistema (opcional, mas recomendado)
sudo apt-get update

# Instale pip se não estiver instalado
sudo apt-get install -y python3-pip

# Instale as dependências do projeto
pip3 install -r requirements.txt

# Se der erro com alguma dependência, instale manualmente:
pip3 install pyserial aiohttp

# Para Raspberry Pi, instale RPi.GPIO do repositório (mais confiável):
sudo apt-get install -y python3-rpi.gpio

# Instale tmux (para gerenciar múltiplos terminais)
sudo apt-get install -y tmux

# Torne os scripts executáveis
chmod +x run_*.sh
```

---

### 4️⃣ Configurar o Sistema

```bash
# Copie o arquivo de configuração de exemplo
cp config.example config.txt

# Edite a configuração
nano config.txt
```

**Configurações importantes para Raspberry Pi**:

```ini
# IPs dos servidores (use 'localhost' se tudo roda na mesma Raspberry Pi)
SERVIDOR_CENTRAL_HOST=localhost
SERVIDOR_CENTRAL_PORT=5000

SERVIDOR_TERREO_HOST=localhost
SERVIDOR_TERREO_PORT=5001

SERVIDOR_ANDAR1_HOST=localhost
SERVIDOR_ANDAR1_PORT=5002

SERVIDOR_ANDAR2_HOST=localhost
SERVIDOR_ANDAR2_PORT=5003

# Porta MODBUS (verifique qual porta USB está sendo usada)
MODBUS_PORT=/dev/ttyUSB0
MODBUS_BAUDRATE=115200
MODBUS_TIMEOUT=0.5
MODBUS_RETRIES=3

# Matrícula (4 últimos dígitos)
MATRICULA=1606

# Outras configurações...
```

**Para verificar a porta USB do MODBUS**:
```bash
ls -l /dev/ttyUSB*
# ou
ls -l /dev/ttyACM*
```

Salve com `Ctrl+X`, depois `Y`, depois `Enter`.

---

## 🖥️ Executar com TMUX (Recomendado)

### Método 1: Script Automático (Mais Fácil)

Vou criar um script que inicia tudo automaticamente no tmux:

```bash
# Use o script de inicialização automática
./start_all_servers.sh
```

### Método 2: Manual com TMUX

#### Iniciar sessão tmux

```bash
# Criar nova sessão chamada 'estacionamento'
tmux new-session -s estacionamento
```

#### Dentro do tmux, criar 4 painéis:

```bash
# Você está no painel 0 (Servidor Central)
./run_servidor_central.sh

# Divida a tela horizontalmente (Ctrl+B, depois ")
# Pressione: Ctrl+B e depois pressione "
# Agora você está no painel 1 (Servidor Térreo)
./run_servidor_terreo.sh

# Volte ao painel 0 (Ctrl+B, depois seta para cima)
# Divida verticalmente (Ctrl+B, depois %)
# Pressione: Ctrl+B e depois pressione %
# Agora você está no painel 2 (Servidor 1º Andar)
./run_servidor_andar1.sh

# Vá para o painel 1 (Ctrl+B, depois seta para baixo)
# Divida verticalmente (Ctrl+B, depois %)
# Pressione: Ctrl+B e depois pressione %
# Agora você está no painel 3 (Servidor 2º Andar)
./run_servidor_andar2.sh
```

#### Navegação no tmux:

| Comando | Ação |
|---------|------|
| `Ctrl+B` + `↑/↓/←/→` | Navegar entre painéis |
| `Ctrl+B` + `"` | Dividir horizontalmente |
| `Ctrl+B` + `%` | Dividir verticalmente |
| `Ctrl+B` + `d` | Desconectar da sessão (mantém rodando) |
| `Ctrl+B` + `x` | Fechar painel atual |
| `Ctrl+B` + `:kill-session` | Encerrar tudo |

#### Reconectar à sessão:

Se você desconectar (Ctrl+B + d) ou perder a conexão SSH:

```bash
# Conecte novamente via SSH
ssh pi@192.168.0.100

# Liste sessões tmux
tmux ls

# Reconecte à sessão
tmux attach-session -t estacionamento
```

---

## 🔧 Script de Inicialização Automática

Vou criar um script que inicia tudo automaticamente:

**Conteúdo do script `start_all_servers.sh`** (já criado abaixo):

```bash
#!/bin/bash

echo "Iniciando Sistema de Estacionamento com TMUX..."
echo ""

# Criar nova sessão tmux (ou reconectar se já existir)
tmux new-session -d -s estacionamento

# Configurar layout com 4 painéis
tmux split-window -h -t estacionamento
tmux split-window -v -t estacionamento:0.0
tmux split-window -v -t estacionamento:0.2

# Executar servidores em cada painel
tmux send-keys -t estacionamento:0.0 'cd ~/Estacionamento_embarcados && ./run_servidor_central.sh' C-m
tmux send-keys -t estacionamento:0.1 'cd ~/Estacionamento_embarcados && sleep 2 && ./run_servidor_terreo.sh' C-m
tmux send-keys -t estacionamento:0.2 'cd ~/Estacionamento_embarcados && sleep 2 && ./run_servidor_andar1.sh' C-m
tmux send-keys -t estacionamento:0.3 'cd ~/Estacionamento_embarcados && sleep 2 && ./run_servidor_andar2.sh' C-m

echo "✅ Todos os servidores foram iniciados!"
echo ""
echo "Para acessar a interface, execute:"
echo "  tmux attach-session -t estacionamento"
echo ""
echo "Para navegar entre painéis:"
echo "  Ctrl+B + setas (↑/↓/←/→)"
echo ""
echo "Para desconectar (mantém rodando):"
echo "  Ctrl+B + d"
echo ""
echo "Para encerrar tudo:"
echo "  Ctrl+B + :kill-session"
```

---

## 🔄 Alternativa: Usar Screen

Se preferir usar `screen` em vez de `tmux`:

### Instalar screen:

```bash
sudo apt-get install -y screen
```

### Uso básico:

```bash
# Iniciar nova sessão
screen -S estacionamento

# Criar nova janela: Ctrl+A, depois C
# Navegar entre janelas: Ctrl+A, depois N (próxima) ou P (anterior)
# Desconectar: Ctrl+A, depois D
# Reconectar: screen -r estacionamento
```

---

## 🎯 Configuração para Múltiplas Raspberry Pi

Se você tem **4 Raspberry Pi** diferentes (uma para cada servidor):

### Raspberry Pi 1 (Central) - IP: 192.168.0.100

```bash
# config.txt
SERVIDOR_CENTRAL_HOST=0.0.0.0
SERVIDOR_CENTRAL_PORT=5000
```

```bash
# Executar apenas:
./run_servidor_central.sh
```

### Raspberry Pi 2 (Térreo) - IP: 192.168.0.101

```bash
# config.txt
SERVIDOR_CENTRAL_HOST=192.168.0.100  # IP do Central
SERVIDOR_TERREO_HOST=0.0.0.0
MODBUS_PORT=/dev/ttyUSB0
```

```bash
# Executar apenas:
./run_servidor_terreo.sh
```

### Raspberry Pi 3 (1º Andar) - IP: 192.168.0.102

```bash
# config.txt
SERVIDOR_CENTRAL_HOST=192.168.0.100  # IP do Central
SERVIDOR_ANDAR1_HOST=0.0.0.0
```

```bash
# Executar apenas:
./run_servidor_andar1.sh
```

### Raspberry Pi 4 (2º Andar) - IP: 192.168.0.103

```bash
# config.txt
SERVIDOR_CENTRAL_HOST=192.168.0.100  # IP do Central
SERVIDOR_ANDAR2_HOST=0.0.0.0
```

```bash
# Executar apenas:
./run_servidor_andar2.sh
```

---

## 🐛 Solução de Problemas

### ⚠️ IMPORTANTE: Problema de Importação Resolvido!

Os scripts foram **atualizados** para corrigir automaticamente o erro:
```
ModuleNotFoundError: No module named 'servidor_central'
```

**Todos os scripts `run_*.sh` agora**:
- ✅ Configuram o `PYTHONPATH` automaticamente
- ✅ Executam os módulos corretamente com `python3 -m`
- ✅ Funcionam em qualquer ambiente (venv ou sistema)

**Se copiar o projeto novamente**, use a versão atualizada!

---

### Erro: "ModuleNotFoundError" (se ainda ocorrer)

```bash
# Configure manualmente o PYTHONPATH
cd ~/Estacionamento_embarcados
export PYTHONPATH="${PYTHONPATH}:$(pwd)"

# Execute os servidores
./start_all_servers.sh

# OU execute diretamente
python3 -m servidor_central.interface
```

### Erro: "Permission denied" ao acessar GPIO

```bash
# Adicione seu usuário ao grupo gpio
sudo usermod -a -G gpio $USER

# Deslogue e logue novamente
exit

# Ou execute com sudo (não recomendado)
sudo ./run_servidor_terreo.sh
```

### Erro: "Connection refused" ao conectar servidores

```bash
# Verifique se o Servidor Central está rodando
ps aux | grep servidor_central

# Verifique se as portas estão abertas
netstat -tuln | grep -E '5000|5001|5002|5003'

# Verifique configuração de firewall (se houver)
sudo ufw status
```

### MODBUS não funciona

```bash
# Verifique se o dispositivo USB está conectado
ls -l /dev/ttyUSB*

# Dê permissão ao dispositivo
sudo chmod 666 /dev/ttyUSB0

# Ou adicione usuário ao grupo dialout
sudo usermod -a -G dialout $USER
```

### Sistema muito lento

```bash
# Libere memória
free -h

# Reinicie a Raspberry Pi se necessário
sudo reboot
```

---

## 📖 Guia Completo de Problemas

Para soluções detalhadas de todos os problemas, consulte:

👉 **[SOLUCAO_PROBLEMAS.md](SOLUCAO_PROBLEMAS.md)** - Guia completo com todas as soluções!

---

## 📊 Monitoramento

### Ver logs em tempo real:

```bash
# No painel do servidor desejado, os logs aparecem automaticamente
# Para rolar a tela no tmux: Ctrl+B + [ (depois use setas, Page Up/Down, q para sair)
```

### Verificar se servidores estão rodando:

```bash
# Verifique processos Python
ps aux | grep python

# Verifique portas abertas
netstat -tuln | grep -E '5000|5001|5002|5003'
```

### Ver uso de recursos:

```bash
# CPU e Memória
htop

# Ou
top
```

---

## 🔒 Segurança

### Recomendações:

1. **Altere a senha padrão da Raspberry Pi**:
   ```bash
   passwd
   ```

2. **Use chaves SSH** em vez de senha:
   ```bash
   # No seu computador local:
   ssh-copy-id pi@192.168.0.100
   ```

3. **Não exponha para a internet** sem firewall/VPN adequados

---

## 🚀 Inicialização Automática (Opcional)

Para iniciar automaticamente ao ligar a Raspberry Pi:

### Usando systemd:

Crie o arquivo `/etc/systemd/system/estacionamento.service`:

```ini
[Unit]
Description=Sistema de Estacionamento
After=network.target

[Service]
Type=forking
User=pi
WorkingDirectory=/home/pi/Estacionamento_embarcados
ExecStart=/home/pi/Estacionamento_embarcados/start_all_servers.sh
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

```bash
# Habilite o serviço
sudo systemctl enable estacionamento.service

# Inicie o serviço
sudo systemctl start estacionamento.service

# Verifique status
sudo systemctl status estacionamento.service
```

---

## 📝 Resumo dos Comandos

### Copiar para Raspberry Pi:
```bash
scp -r Estacionamento_embarcados pi@192.168.0.100:~/
```

### Conectar via SSH:
```bash
ssh pi@192.168.0.100
```

### Instalar e Configurar:
```bash
cd ~/Estacionamento_embarcados
pip3 install -r requirements.txt
sudo apt-get install -y tmux
chmod +x *.sh
cp config.example config.txt
nano config.txt
```

### Executar (com tmux):
```bash
./start_all_servers.sh
tmux attach-session -t estacionamento
```

### Navegar no tmux:
- `Ctrl+B` + setas: mover entre painéis
- `Ctrl+B` + `d`: desconectar (mantém rodando)
- `Ctrl+B` + `:kill-session`: encerrar tudo

### Reconectar:
```bash
ssh pi@192.168.0.100
tmux attach-session -t estacionamento
```

---

## 🎓 Dicas Extras

1. **Mantenha a sessão rodando**: Use `tmux` ou `screen` para que os servidores continuem rodando mesmo se você desconectar

2. **Logs**: Todos os logs aparecem nos painéis do tmux

3. **Ordem**: O script aguarda 2 segundos entre cada servidor para garantir inicialização correta

4. **Reconexão SSH**: Se perder a conexão SSH, basta reconectar e fazer `tmux attach`

5. **Backup**: Faça backup do `config.txt` após configurar

---

**Pronto! Agora você pode gerenciar todo o sistema remotamente via SSH! 🎉**

