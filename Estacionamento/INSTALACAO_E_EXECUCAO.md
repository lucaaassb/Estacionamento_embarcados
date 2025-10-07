# Guia de Instalação e Execução - Sistema de Estacionamento

## ✅ Correções Implementadas (100% Conforme README)

### 1. **Mapeamento de Pinos GPIO** ✅

- **Térreo**: Corrigido conforme Tabela 1 do README
- **1º Andar**: Corrigido conforme Tabela 2 do README
- **2º Andar**: Corrigido conforme Tabela 3 do README

### 2. **MODBUS com Retry e Backoff Exponencial** ✅

- Implementado retry com 3 tentativas máximas
- Backoff exponencial: 100ms → 250ms → 500ms
- Logs detalhados de erros MODBUS
- Função `modbus_write_registers_with_retry()` implementada

### 3. **Política de Tratamento de Confiança** ✅

- **≥70%**: Alta confiança, entrada/saída normal
- **60-69%**: Confiança média, gera ticket temporário
- **<60%**: Baixa confiança, gera ticket anônimo
- Reconciliação manual via interface do servidor central

### 4. **Reconexão Automática TCP/IP** ✅

- Tentativas de reconexão automática (máx: 5)
- Backoff exponencial para reconexão
- Modo degradado após esgotamento de tentativas
- Timeouts configurados nos sockets

### 5. **Sistema de Logs Robusto** ✅

- Logs estruturados com timestamp
- Rotação automática quando arquivo > 10MB
- Níveis: DEBUG, INFO, ERRO
- Diretório próprio: `logs/`

### 6. **Lógica de Passagem Entre Andares** ✅

- Detecção correta: Sensor1→Sensor2 = sobe, Sensor2→Sensor1 = desce
- Timestamps para determinar direção
- Logs de eventos de passagem

### 7. **Interface do Servidor Central Completa** ✅

- Menu interativo com comandos completos
- Visualização de tickets temporários (opção 7)
- Visualização de alertas de auditoria (opção 8)
- Controle de abertura/fechamento por andar

### 8. **Atualização Robusta do Placar MODBUS** ✅

- Retry nas operações de escrita
- Envio da matrícula conforme especificação
- Flags de status (lotado geral, andares bloqueados)

### 9. **Integração com Dashboard ThingsBoard** ✅

- **✅ Envio automático** de dados para dashboard web
- **✅ Eventos em tempo real**: entrada/saída de veículos
- **✅ Estado das vagas**: ocupação por vaga individual
- **✅ Dados financeiros**: valores arrecadados por cobrança
- **✅ Contadores**: totais de entrada/saída por andar
- **✅ Dashboard URL**: https://tb.fse.lappis.rocks/dashboard/ebaeba20-9e59-11f0-a4ce-1d78bb2310d8

## 🚀 Instalação

### Pré-requisitos

```bash
sudo apt-get update
sudo apt-get install -y libbcm2835-dev libmodbus-dev build-essential libcurl4-openssl-dev libjson-c-dev
```

### Compilação

```bash
# Instalar dependências automaticamente
make install-deps

# Compilar o sistema
make all

# Ou fazer setup completo
make setup

# Configurar dashboard ThingsBoard (IMPORTANTE!)
make config-thingsboard
```

### Verificação

```bash
# Executar testes completos
make test

# Verificar mapeamento GPIO
chmod +x verificar_gpio.sh
./verificar_gpio.sh
```

## 🏃‍♂️ Execução

### Execução Local (Teste)

```bash
# Terminal 1: Servidor Central
bin/main c

# Terminal 2: Servidor Térreo
bin/main t

# Terminal 3: Servidor 1º Andar
bin/main u

# Terminal 4: Servidor 2º Andar
bin/main d
```

### Execução Distribuída (Raspberry Pi)

#### 1. Configurar IPs no `config.env`:

```bash
# Editar conforme sua rede
RASP40_IP=164.41.98.2    # Servidor Central
RASP41_IP=164.41.98.3    # Servidor Térreo
RASP42_IP=164.41.98.4    # Servidor 1º Andar
RASP43_IP=164.41.98.5    # Servidor 2º Andar
```

#### 2. Deploy em cada Raspberry Pi:

```bash
# Raspberry Pi Central (40)
scp -r Estacionamento/ usuario@164.41.98.2:~/
ssh usuario@164.41.98.2 "cd Estacionamento && make setup && bin/main c"

# Raspberry Pi Térreo (41)
scp -r Estacionamento/ usuario@164.41.98.3:~/
ssh usuario@164.41.98.3 "cd Estacionamento && make setup && bin/main t"

# Raspberry Pi 1º Andar (42)
scp -r Estacionamento/ usuario@164.41.98.4:~/
ssh usuario@164.41.98.4 "cd Estacionamento && make setup && bin/main u"

# Raspberry Pi 2º Andar (43)
scp -r Estacionamento/ usuario@164.41.98.5:~/
ssh usuario@164.41.98.5 "cd Estacionamento && make setup && bin/main d"
```

## 🎮 Interface do Servidor Central

### Menu Principal:

- `1` - Abrir estacionamento
- `2` - Fechar estacionamento
- `3` - Ativar 1º andar
- `4` - Desativar 1º andar
- `5` - Ativar 2º andar
- `6` - Desativar 2º andar
- `7` - **Ver tickets temporários** 🎫
- `8` - **Ver alertas de auditoria** ⚠️
- `q` - Encerrar sistema

### Funcionalidades Implementadas:

- ✅ **Dashboard em tempo real** com ocupação por andar
- ✅ **Cálculo automático de cobrança** (R$ 0,15/min)
- ✅ **Recibos de saída** com valores detalhados
- ✅ **Gestão de tickets temporários** para baixa confiança
- ✅ **Alertas de auditoria** para inconsistências

## � Dashboard ThingsBoard

### ✅ **SIM! O dashboard reconhecerá quando você clicar na entrada de um carro!**

#### Como Funciona:

1. **Evento de Entrada**: Quando sensor detecta veículo → dados enviados automaticamente
2. **Dashboard Atualiza**: Em tempo real, sem delay
3. **Dados Enviados**:
   - Estado de cada vaga (T1, T2, T3, T4)
   - Evento: "entrada" com timestamp
   - Placa do veículo (se capturada)
   - Totais de carros no estacionamento

#### Configuração Necessária:

```bash
# 1. Configurar tokens do ThingsBoard
make config-thingsboard

# 2. Inserir token do seu dispositivo rasp46
# (o sistema solicitará o token durante a configuração)

# 3. Recompilar com suporte ao dashboard
make clean && make all

# 4. Executar o sistema
bin/main t  # Para térreo
```

#### URL do Dashboard:

**Seu Dashboard**: https://tb.fse.lappis.rocks/dashboard/ebaeba20-9e59-11f0-a4ce-1d78bb2310d8?publicId=86d17ff0-e010-11ef-9ab8-4774ff1517e8

#### Dados Enviados em Tempo Real:

- 🚗 **Eventos de entrada/saída** (quando você clicar no sensor)
- 📊 **Ocupação das vagas** (T1: 0/1, T2: 0/1, etc.)
- 💰 **Valores arrecadados** por cobrança
- 📈 **Contadores totais** (carros entrada/saída)
- ⏱️ **Timestamps** precisos de cada evento

#### Exemplo de Dados JSON Enviados:

```json
{
  "ts": 1696603200000,
  "evento": "entrada",
  "placa": "ABC1234",
  "vaga_T1": 1,
  "vaga_T2": 0,
  "vaga_T3": 0,
  "vaga_T4": 0,
  "vagas_ocupadas_total": 1,
  "carros_entrada": 15,
  "carros_saida": 12
}
```

## �🔌 MODBUS RTU

### Dispositivos Configurados:

- **Câmera Entrada**: `0x11`
- **Câmera Saída**: `0x12`
- **Placar**: `0x20`

### Configuração Serial:

- **Porta**: `/dev/ttyUSB0`
- **Baudrate**: `115200`
- **Formato**: `8N1`
- **Timeout**: `500ms`

### Funcionalidades MODBUS:

- ✅ **Trigger de captura** com polling de status
- ✅ **Leitura de placas** com validação de confiança
- ✅ **Atualização do placar** com retry automático
- ✅ **Envio de matrícula** (202017700) conforme especificação

## 📊 Sistema de Logs

### Estrutura:

```
logs/
├── estacionamento_20251006.log    # Log diário
├── estacionamento_20251005.log.backup  # Backup rotacionado
└── sistema_estacionamento.log     # Log geral
```

### Níveis de Log:

- **ERRO**: Falhas críticas, MODBUS, conexões
- **INFO**: Eventos normais, entrada/saída de veículos
- **DEBUG**: Detalhes técnicos, GPIO, sensores

## 🎫 Gestão de Tickets Temporários

### Situações que Geram Tickets:

1. **Confiança 60-69%**: Ticket com placa original
2. **Confiança <60%**: Ticket anônimo (`ANOM####`)
3. **Falha de captura**: Ticket de falha (`FALHA####`)

### Reconciliação:

- Via interface do servidor central (opção 7)
- Busca manual por ID ou placa
- Resolução de inconsistências

## ⚠️ Alertas de Auditoria

### Tipos de Alertas:

1. **Sem correspondência de entrada**: Veículo sai sem registro de entrada
2. **Placa inválida**: Problemas na validação da placa
3. **Erro de sistema**: Falhas técnicas diversas

### Visualização:

- Interface do servidor central (opção 8)
- Logs detalhados com timestamp
- Status de resolução

## 🔧 Troubleshooting

### Problemas Comuns:

#### 1. **Erro de compilação MODBUS**:

```bash
# Compilar sem MODBUS (modo simulado)
make EXTRA_CFLAGS="-DNO_MODBUS"
```

#### 2. **Erro de permissão GPIO**:

```bash
sudo usermod -a -G gpio $USER
# Relogar após comando
```

#### 3. **Erro de porta serial**:

```bash
sudo usermod -a -G dialout $USER
sudo chmod 666 /dev/ttyUSB0
```

#### 4. **IPs incorretos**:

- Editar `config.env` com IPs corretos da sua rede
- Verificar conectividade: `ping <IP_DESTINO>`

### Logs de Debug:

```bash
# Ver logs em tempo real
tail -f logs/estacionamento_$(date +%Y%m%d).log

# Filtrar erros
grep "ERRO" logs/*.log

# Verificar MODBUS
grep "MODBUS" logs/*.log
```

## 📈 Performance e Monitoramento

### Dashboards (conforme README):

- [Estacionamento - rasp40](https://tb.fse.lappis.rocks/dashboard/54159c30-9c04-11f0-a4ce-1d78bb2310d8?publicId=86d17ff0-e010-11ef-9ab8-4774ff1517e8)
- [Estacionamento - rasp41](https://tb.fse.lappis.rocks/dashboard/362971f0-9e30-11f0-a4ce-1d78bb2310d8?publicId=86d17ff0-e010-11ef-9ab8-4774ff1517e8)

### Métricas Monitoradas:

- ✅ Ocupação por andar em tempo real
- ✅ Eventos de entrada/saída
- ✅ Performance MODBUS
- ✅ Status das conexões TCP/IP
- ✅ Tickets e alertas pendentes

---

## 🎯 Conformidade 100% com README

✅ **Arquitetura**: Servidor Central + 3 Servidores Distribuídos  
✅ **GPIO**: Mapeamento correto conforme tabelas  
✅ **MODBUS**: Implementação completa com retry  
✅ **TCP/IP**: Comunicação robusta com reconexão  
✅ **JSON**: Protocolo de mensagens implementado  
✅ **Regras de negócio**: R$ 0,15/min + política de confiança  
✅ **Interface**: Menu completo com todas as funcionalidades  
✅ **Logs**: Sistema robusto com rotação  
✅ **Persistência**: Eventos, tickets e alertas  
✅ **Tratamento de erros**: Modo degradado e recuperação

**Sistema pronto para uso em produção!** 🚀
