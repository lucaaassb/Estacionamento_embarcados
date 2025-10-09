# Guia de Instalação e Execução

## Pré-requisitos

- Raspberry Pi com Raspbian/Raspberry Pi OS
- Python 3.7 ou superior
- Acesso root ou permissões GPIO configuradas
- Porta serial USB (para MODBUS, se disponível)

## Instalação

### 1. Clone ou baixe o projeto

```bash
cd /home/pi
git clone [URL_DO_REPOSITORIO]
cd Estacionamento_embarcados
```

### 2. Instale as dependências Python

```bash
pip3 install -r requirements.txt
```

Ou instale manualmente:

```bash
sudo apt-get update
sudo apt-get install python3-pip python3-rpi.gpio
pip3 install pyserial asyncio aiohttp
```

### 3. Configure o sistema

Copie e edite o arquivo de configuração:

```bash
cp config.example config.txt
nano config.txt
```

**Configurações importantes**:
- `servidor_central_host`: IP da Raspberry Pi do servidor central
- `modbus_port`: Porta serial do MODBUS (ex: `/dev/ttyUSB0`)
- `matricula`: 4 últimos dígitos da sua matrícula

### 4. Torne os scripts executáveis

```bash
chmod +x run_*.sh
```

## Execução

### Modo de Desenvolvimento (um computador)

Abra 4 terminais diferentes:

**Terminal 1 - Servidor Central**:
```bash
./run_servidor_central.sh
```

**Terminal 2 - Servidor Térreo**:
```bash
./run_servidor_terreo.sh
```

**Terminal 3 - Servidor 1º Andar**:
```bash
./run_servidor_andar1.sh
```

**Terminal 4 - Servidor 2º Andar**:
```bash
./run_servidor_andar2.sh
```

### Modo de Produção (Raspberry Pi distribuídas)

#### Raspberry Pi 1 (Servidor Central)
```bash
./run_servidor_central.sh
```

#### Raspberry Pi 2 (Servidor Térreo)
```bash
./run_servidor_terreo.sh
```

#### Raspberry Pi 3 (Servidor 1º Andar)
```bash
./run_servidor_andar1.sh
```

#### Raspberry Pi 4 (Servidor 2º Andar)
```bash
./run_servidor_andar2.sh
```

## Ordem de Inicialização

É recomendado iniciar os servidores nesta ordem:

1. **Servidor Central** (primeiro)
2. Aguarde 2-3 segundos
3. **Servidores distribuídos** (qualquer ordem)

Os servidores distribuídos tentarão reconectar automaticamente se o Central ainda não estiver pronto.

## Interface do Servidor Central

Após iniciar o Servidor Central, você verá uma interface de monitoramento:

```
================================================================================
                         SISTEMA DE ESTACIONAMENTO
================================================================================

STATUS GERAL:
  Estacionamento: ABERTO
  Situação: Disponível
  Veículos ativos: 0

VAGAS DISPONÍVEIS:
[...]

COMANDOS:
  [1] Atualizar tela
  [2] Fechar/Abrir estacionamento
  [3] Bloquear/Desbloquear 1º andar
  [4] Bloquear/Desbloquear 2º andar
  [5] Ver histórico de saídas
  [6] Ver veículos ativos
  [0] Sair
================================================================================
```

## Verificação de Funcionamento

### Logs

Todos os servidores geram logs detalhados. Verifique se há erros:

- `INFO`: Operação normal
- `WARNING`: Avisos (ex: baixa confiança de LPR)
- `ERROR`: Erros que precisam atenção

### Teste de Comunicação

1. Verifique se o Servidor Central mostra os servidores conectados
2. Teste um comando (ex: bloquear um andar)
3. Observe as mensagens de log em todos os terminais

### Teste de GPIO

**ATENÇÃO**: Teste de GPIO só funciona em Raspberry Pi real!

No modo de desenvolvimento (sem hardware), o sistema usa um mock de GPIO.

## Solução de Problemas

### Erro: "Permission denied" ao acessar GPIO

Execute com sudo:
```bash
sudo ./run_servidor_terreo.sh
```

Ou configure permissões:
```bash
sudo usermod -a -G gpio $USER
sudo reboot
```

### Erro: "MODBUS inativo"

Se não tiver hardware MODBUS conectado:
- O sistema opera em **modo degradado**
- Cancelas funcionam
- LPR usa IDs temporários
- Placar não é atualizado

Isso é normal em ambiente de desenvolvimento.

### Erro: "Connection refused" ao conectar com Central

- Verifique se o Servidor Central está rodando
- Verifique o IP configurado em `config.txt`
- Verifique firewall (se houver)

### Erro: "ModuleNotFoundError"

Instale a dependência faltante:
```bash
pip3 install [nome_do_modulo]
```

## Encerrando o Sistema

Pressione `Ctrl+C` em cada terminal ou:

- No Servidor Central, digite `0` e ENTER
- Nos outros servidores, pressione `Ctrl+C`

## Arquivos de Log

Os logs são exibidos no terminal. Para salvar em arquivo:

```bash
./run_servidor_central.sh 2>&1 | tee central.log
```

## Estrutura de Arquivos Importante

```
Estacionamento_embarcados/
├── comum/                  # Módulos compartilhados
├── servidor_central/       # Servidor Central
├── servidor_terreo/        # Servidor Térreo
├── servidor_andar/         # Servidores dos Andares
├── config.example          # Exemplo de configuração
├── requirements.txt        # Dependências Python
├── run_*.sh               # Scripts de execução
├── INSTALACAO.md          # Este arquivo
└── ARQUITETURA.md         # Documentação completa
```

## Próximos Passos

1. Leia `ARQUITETURA.md` para entender como o sistema funciona
2. Configure seu hardware (GPIO, MODBUS)
3. Ajuste `config.txt` conforme necessário
4. Execute testes com veículos reais

## Suporte

Para dúvidas sobre arquitetura e funcionamento, consulte:
- `ARQUITETURA.md`: Documentação completa
- `README.md`: Especificação do trabalho

## Notas Importantes

- ⚠️ **GPIO**: Funciona apenas em Raspberry Pi real
- ⚠️ **MODBUS**: Requer hardware conectado ou simulador
- ✅ **TCP/IP**: Funciona em qualquer sistema (desenvolvimento)
- ✅ **Modo degradado**: Sistema funciona mesmo sem hardware completo

