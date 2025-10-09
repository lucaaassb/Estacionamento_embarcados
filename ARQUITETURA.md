# Documentação da Arquitetura do Sistema de Estacionamento

## Índice

1. [Visão Geral](#visão-geral)
2. [Arquitetura do Sistema](#arquitetura-do-sistema)
3. [Estrutura de Diretórios](#estrutura-de-diretórios)
4. [Módulos Comuns](#módulos-comuns)
5. [Servidor Central](#servidor-central)
6. [Servidor do Andar Térreo](#servidor-do-andar-térreo)
7. [Servidores dos Andares](#servidores-dos-andares)
8. [Comunicação entre Servidores](#comunicação-entre-servidores)
9. [Protocolo MODBUS](#protocolo-modbus)
10. [Controle de GPIO](#controle-de-gpio)
11. [Fluxos de Operação](#fluxos-de-operação)
12. [Instalação e Execução](#instalação-e-execução)

---

## Visão Geral

Este sistema implementa um **controle distribuído de estacionamento** de 3 andares (Térreo, 1º e 2º andares) utilizando Raspberry Pi. O sistema gerencia:

- **Entrada e saída de veículos** com cancelas automáticas e leitura de placas via câmeras LPR (MODBUS)
- **Monitoramento de vagas** por varredura multiplexada de sensores GPIO
- **Cobrança automática** baseada em tempo de permanência (R$ 0,15/minuto)
- **Placar de vagas** atualizado em tempo real via MODBUS
- **Interface de gerenciamento** com comandos para fechar estacionamento e bloquear andares

### Tecnologias Utilizadas

- **Linguagem**: Python 3
- **Comunicação**: TCP/IP (JSON) entre servidores
- **Bus Serial**: RS485-MODBUS RTU (115200 bps, 8N1)
- **GPIO**: RPi.GPIO para controle de sensores e atuadores
- **Arquitetura**: Sistema distribuído assíncrono (asyncio)

---

## Arquitetura do Sistema

O sistema é composto por **4 servidores** independentes que se comunicam via TCP/IP:

```
┌──────────────────────────────────────────────────────────────┐
│                     SERVIDOR CENTRAL                          │
│  - Consolida informações de todos os andares                  │
│  - Calcula cobranças                                          │
│  - Interface de monitoramento e comandos                      │
│  - Gerencia estado geral do estacionamento                    │
└───────────────────┬──────────────────────────────────────────┘
                    │ TCP/IP (JSON)
        ┌───────────┴─────────────┬─────────────────┐
        │                         │                 │
┌───────▼───────────┐   ┌─────────▼────────┐   ┌───▼──────────┐
│  SERVIDOR TÉRREO  │   │  SERVIDOR 1º ANR │   │ SERVIDOR 2º  │
│  - Cancelas       │   │  - Vagas (8)     │   │ - Vagas (8)  │
│  - Vagas (4)      │   │  - Passagem ↕    │   │ - Passagem ↕ │
│  - MODBUS Master  │   │                  │   │              │
└─────────┬─────────┘   └──────────────────┘   └──────────────┘
          │
          │ RS485-MODBUS
    ┌─────┴─────┬──────────────┬────────────┐
    │           │              │            │
┌───▼───┐  ┌────▼────┐  ┌──────▼──────┐    │
│ Câmera│  │ Câmera  │  │   Placar    │    │
│ LPR   │  │  LPR    │  │   de Vagas  │    │
│Entrada│  │  Saída  │  │             │    │
│(0x11) │  │ (0x12)  │  │   (0x20)    │    │
└───────┘  └─────────┘  └─────────────┘    │
```

---

## Estrutura de Diretórios

```
Estacionamento_embarcados/
│
├── comum/                          # Módulos compartilhados
│   ├── __init__.py
│   ├── config.py                   # Gerenciamento de configurações
│   ├── mensagens.py                # Definição de mensagens TCP/IP
│   ├── comunicacao.py              # Cliente e Servidor TCP assíncrono
│   ├── modbus_client.py            # Cliente MODBUS RTU
│   └── gpio_handler.py             # Controle de GPIO e sensores
│
├── servidor_central/               # Servidor Central
│   ├── __init__.py
│   ├── servidor_central.py         # Lógica principal do servidor
│   └── interface.py                # Interface CLI de monitoramento
│
├── servidor_terreo/                # Servidor do Térreo
│   ├── __init__.py
│   └── servidor_terreo.py          # Controle de cancelas e MODBUS
│
├── servidor_andar/                 # Servidores dos Andares
│   ├── __init__.py
│   └── servidor_andar.py           # Lógica para 1º e 2º andares
│
├── config.example                  # Exemplo de configuração
├── requirements.txt                # Dependências Python
│
├── run_servidor_central.sh         # Script para iniciar Central
├── run_servidor_terreo.sh          # Script para iniciar Térreo
├── run_servidor_andar1.sh          # Script para iniciar 1º Andar
├── run_servidor_andar2.sh          # Script para iniciar 2º Andar
│
├── README.md                       # Especificação do trabalho
└── ARQUITETURA.md                  # Este documento
```

---

## Módulos Comuns

Os módulos comuns são compartilhados por todos os servidores e fornecem funcionalidades essenciais.

### `comum/config.py`

**Responsabilidade**: Gerenciar configurações do sistema.

**Principais funcionalidades**:
- Carrega configurações de arquivo ou usa valores padrão
- Armazena configurações de rede (hosts, portas)
- Armazena configurações MODBUS (porta serial, baudrate, endereços)
- Armazena configurações de vagas e preços

**Classe principal**: `Config`

```python
config = Config()
porta_central = config.get('servidor_central_port', 5000)
```

---

### `comum/mensagens.py`

**Responsabilidade**: Definir tipos e formatos de mensagens trocadas entre servidores.

**Tipos de mensagens**:
- `ENTRADA_OK`: Veículo entrou no estacionamento
- `SAIDA_OK`: Veículo saiu do estacionamento
- `VAGA_STATUS`: Atualização de status de vagas
- `PASSAGEM_ANDAR`: Veículo passou entre andares
- `CALCULAR_VALOR`: Solicitar cálculo de cobrança
- `RESPOSTA_VALOR`: Resposta com valor calculado
- `COMANDO_CANCELA`: Comando para abrir/fechar cancela
- `ATUALIZAR_PLACAR`: Atualizar informações no placar
- `FECHAR_ESTACIONAMENTO`: Fechar/abrir estacionamento
- `BLOQUEAR_ANDAR`: Bloquear/desbloquear andar

**Classe principal**: `Mensagem`

```python
# Exemplo de criação de mensagem
mensagem = Mensagem.criar_entrada_ok("ABC1234", confianca=95, andar=0)
```

---

### `comum/comunicacao.py`

**Responsabilidade**: Gerenciar comunicação TCP/IP assíncrona entre servidores.

**Componentes**:

#### `ServidorTCP`
- Aceita conexões de clientes
- Recebe mensagens em formato JSON
- Chama callback para processar mensagens
- Envia respostas quando necessário

**Uso**:
```python
servidor = ServidorTCP(host, port, callback_processar_mensagem)
await servidor.iniciar()
```

#### `ClienteTCP`
- Conecta a servidores remotos
- Envia mensagens em formato JSON
- Pode aguardar respostas
- Reconecta automaticamente em caso de falha

**Uso**:
```python
cliente = ClienteTCP(host, port)
await cliente.conectar()
await cliente.enviar(mensagem, esperar_resposta=True)
```

**Protocolo de comunicação**:
1. Envia 4 bytes com o tamanho da mensagem (big-endian)
2. Envia a mensagem JSON
3. Se esperar resposta, aguarda mesmo formato de volta

---

### `comum/modbus_client.py`

**Responsabilidade**: Implementar cliente MODBUS RTU para comunicação serial.

**Componentes**:

#### `ModbusClient`
Cliente MODBUS RTU básico que implementa:
- **Função 0x03**: Read Holding Registers
- **Função 0x10**: Write Multiple Registers
- Cálculo de CRC-16 MODBUS
- Adição de matrícula antes do CRC
- Sistema de retentativas com backoff exponencial

**Uso**:
```python
modbus = ModbusClient('/dev/ttyUSB0', 115200)
modbus.conectar()
modbus.set_matricula('1234')
valores = modbus.read_holding_registers(0x11, 0, 7)
```

#### `CameraLPR`
Interface de alto nível para câmeras LPR:
- Dispara captura de placa (escreve 1 no trigger)
- Faz polling do status até OK ou ERRO
- Lê placa (4 registradores, 8 chars ASCII)
- Lê confiança (0-100%)
- Zera trigger após leitura

**Fluxo de captura**:
```python
camera = CameraLPR(modbus, 0x11)
resultado = camera.capturar_placa(timeout=2.0)
if resultado:
    placa, confianca = resultado
```

#### `PlacarVagas`
Interface para atualizar placar de vagas:
- Escreve 13 registradores de uma vez
- Vagas livres por tipo (PNE, Idoso+, Comuns) e andar
- Número de carros por andar
- Flags de status (lotado, bloqueado)

**Uso**:
```python
placar = PlacarVagas(modbus, 0x20)
placar.atualizar(vagas_livres, num_carros, lotado_geral, lotado_andar1, lotado_andar2)
```

---

### `comum/gpio_handler.py`

**Responsabilidade**: Gerenciar GPIO da Raspberry Pi.

**Componentes**:

#### `GPIOHandler`
Classe base para operações GPIO:
- Configura pinos como entrada/saída
- Lê e escreve valores
- Adiciona/remove interrupções
- Cleanup ao finalizar

#### `VarredorVagas`
Implementa varredura multiplexada de vagas:
- Usa pinos de endereço para selecionar vaga (multiplexador)
- Lê sensor de vaga para verificar ocupação
- Suporta 2^N vagas (N = número de pinos de endereço)
- Térreo: 2 bits = 4 vagas
- Andares: 3 bits = 8 vagas

**Funcionamento**:
```python
varredor = VarredorVagas(gpio, [pino_end1, pino_end2], pino_sensor)
estados = varredor.varrer_todas()  # [True, False, True, False] = [ocupada, livre, ocupada, livre]
```

#### `ControleCancela`
Controla cancelas de entrada/saída:
- Gerencia motor da cancela
- Monitora sensores de abertura e fechamento
- Máquina de estados: FECHADA → ABRINDO → ABERTA → FECHANDO → FECHADA
- Timeout de segurança

**Estados**:
- `FECHADA`: Cancela completamente fechada
- `ABRINDO`: Motor ligado, aguardando sensor de abertura
- `ABERTA`: Cancela completamente aberta
- `FECHANDO`: Motor ligado, aguardando sensor de fechamento

#### `SensorPassagem`
Detecta passagem entre andares:
- Dois sensores em sequência
- Sequência 1→2: veículo subindo
- Sequência 2→1: veículo descendo
- Timeout para resetar sequência

---

## Servidor Central

**Arquivo**: `servidor_central/servidor_central.py`

**Responsabilidades**:
1. Consolidar informações de todos os andares
2. Calcular cobranças por tempo de permanência
3. Gerenciar estado global do estacionamento
4. Controlar fechamento e bloqueio de andares
5. Manter histórico de veículos

### Classes Principais

#### `RegistroVeiculo`
Armazena informações de um veículo:
- Placa
- Timestamp de entrada
- Andar atual
- Confiança da leitura
- Timestamp de saída (quando aplicável)
- Valor pago
- Tempo de permanência

#### `ServidorCentral`
Servidor principal que gerencia todo o sistema.

**Atributos importantes**:
- `veiculos_ativos`: Dict de veículos atualmente no estacionamento
- `historico`: Lista de registros completos (entrada + saída)
- `vagas_totais`: Configuração de vagas por andar
- `vagas_livres`: Vagas disponíveis em tempo real
- `num_carros`: Contador de carros por andar
- `estacionamento_fechado`: Flag de estacionamento fechado
- `andar1_bloqueado`, `andar2_bloqueado`: Flags de bloqueio

**Métodos principais**:

- `processar_mensagem()`: Processa mensagens recebidas via TCP/IP
- `_processar_entrada()`: Registra entrada de veículo, valida se há vagas
- `_processar_saida()`: Registra saída, calcula cobrança, gera recibo
- `_processar_vaga_status()`: Atualiza status de vagas de um andar
- `_calcular_valor()`: Calcula valor a pagar por um veículo
- `_calcular_cobranca()`: Calcula tempo e valor baseado em timestamps
- `_atualizar_placar()`: Envia atualização para o placar via servidor térreo
- `fechar_estacionamento()`: Fecha/abre o estacionamento
- `bloquear_andar()`: Bloqueia/desbloqueia um andar específico
- `obter_status()`: Retorna status completo do sistema

**Cálculo de cobrança**:
```python
tempo_segundos = (saida - entrada).total_seconds()
tempo_minutos = int(tempo_segundos / 60) + (1 if tempo_segundos % 60 > 0 else 0)
valor = tempo_minutos * PRECO_POR_MINUTO  # R$ 0,15/min
```

---

### Interface CLI

**Arquivo**: `servidor_central/interface.py`

**Responsabilidade**: Fornecer interface de linha de comando para monitoramento e controle.

**Classe**: `InterfaceCLI`

**Tela principal exibe**:
- Status geral (aberto/fechado, lotado/disponível)
- Vagas disponíveis por tipo e andar
- Número de carros por andar
- Veículos ativos

**Comandos disponíveis**:
1. Atualizar tela
2. Fechar/Abrir estacionamento
3. Bloquear/Desbloquear 1º andar
4. Bloquear/Desbloquear 2º andar
5. Ver histórico de saídas (últimos 10)
6. Ver veículos ativos
0. Sair

**Atualização automática**: A tela é atualizada automaticamente a cada 5 segundos.

---

## Servidor do Andar Térreo

**Arquivo**: `servidor_terreo/servidor_terreo.py`

**Responsabilidades**:
1. Controlar cancelas de entrada e saída
2. Ler placas via câmeras LPR (MODBUS)
3. Atualizar placar de vagas (MODBUS)
4. Varrer sensores de vagas do térreo
5. Comunicar eventos ao servidor central

### Configuração GPIO

Conforme Tabela 1 do README:

```python
GPIO_TERREO = {
    'endereco_01': 17,          # Bit 0 do endereço
    'endereco_02': 18,          # Bit 1 do endereço
    'sensor_vaga': 8,           # Sensor multiplexado
    'sensor_abertura_cancela_entrada': 7,
    'sensor_fechamento_cancela_entrada': 1,
    'motor_cancela_entrada': 23,
    'sensor_abertura_cancela_saida': 12,
    'sensor_fechamento_cancela_saida': 25,
    'motor_cancela_saida': 24
}
```

### Classe Principal

#### `ServidorTerreo`

**Atributos**:
- `gpio`: Handler de GPIO
- `varredor`: Varredor de 4 vagas (2 bits)
- `cancela_entrada`, `cancela_saida`: Controles de cancela
- `modbus`: Cliente MODBUS
- `camera_entrada`, `camera_saida`: Interfaces de câmeras LPR
- `placar`: Interface do placar
- `modbus_ativo`: Flag indicando se MODBUS está operacional

**Tarefas assíncronas**:

#### 1. `tarefa_varredura_vagas()`
- Varre vagas do térreo periodicamente (1 segundo)
- Detecta mudanças de ocupação
- Envia atualização ao servidor central

#### 2. `tarefa_cancela_entrada()`
Loop de gerenciamento da entrada:

1. Aguarda sensor de presença ativar
2. Dispara captura de placa via câmera LPR (MODBUS)
3. Se falhar ou baixa confiança, gera ID temporário
4. Envia evento de entrada ao servidor central
5. Se autorizado:
   - Abre cancela
   - Aguarda veículo passar
   - Fecha cancela

**Tratamento de baixa confiança**:
- Se confiança < 70%, registra mas permite entrada
- Se falha total, usa placa temporária `TEMP{timestamp}`

#### 3. `tarefa_cancela_saida()`
Loop de gerenciamento da saída:

1. Aguarda sensor de presença ativar
2. Dispara captura de placa via câmera LPR (MODBUS)
3. Envia evento de saída ao servidor central
4. Recebe cálculo de valor
5. Exibe recibo de pagamento
6. Abre cancela
7. Aguarda veículo passar
8. Fecha cancela

**Recibo exibido**:
```
==================================================
RECIBO DE PAGAMENTO
==================================================
Placa: ABC1234
Tempo: 45 minutos
Valor: R$ 6.75
==================================================
```

**Modo degradado**: Se MODBUS falhar na inicialização, o sistema opera sem LPR, permitindo entrada/saída com IDs temporários.

---

## Servidores dos Andares

**Arquivo**: `servidor_andar/servidor_andar.py`

**Responsabilidades**:
1. Varrer sensores de vagas (8 vagas por andar)
2. Detectar passagem entre andares
3. Comunicar eventos ao servidor central

### Configuração GPIO

#### 1º Andar (Tabela 2):
```python
GPIO_ANDAR1 = {
    'endereco_01': 16,
    'endereco_02': 20,
    'endereco_03': 21,
    'sensor_vaga': 27,
    'sensor_passagem_1': 22,
    'sensor_passagem_2': 11
}
```

#### 2º Andar (Tabela 3):
```python
GPIO_ANDAR2 = {
    'endereco_01': 0,
    'endereco_02': 5,
    'endereco_03': 6,
    'sensor_vaga': 13,
    'sensor_passagem_1': 19,
    'sensor_passagem_2': 26
}
```

### Classe Principal

#### `ServidorAndar`

**Construtor**:
```python
servidor = ServidorAndar(config, numero_andar)  # numero_andar = 1 ou 2
```

**Atributos**:
- `numero_andar`: 1 ou 2
- `varredor`: Varredor de 8 vagas (3 bits)
- `sensor_passagem`: Detector de passagem entre andares
- `vagas_ocupadas`: Estado atual das vagas

**Tarefas assíncronas**:

#### 1. `tarefa_varredura_vagas()`
- Varre 8 vagas periodicamente (1 segundo)
- Detecta mudanças de ocupação
- Calcula vagas livres por tipo
- Envia atualização ao servidor central

#### 2. Callbacks de passagem

##### `_callback_subindo()`
Chamado quando detecta sequência 1→2:
- Envia evento "subindo" ao servidor central
- Central atualiza contadores de carros

##### `_callback_descendo()`
Chamado quando detecta sequência 2→1:
- Envia evento "descendo" ao servidor central
- Central atualiza contadores de carros

**Execução**:
```bash
python3 servidor_andar/servidor_andar.py 1  # 1º Andar
python3 servidor_andar/servidor_andar.py 2  # 2º Andar
```

---

## Comunicação entre Servidores

### Protocolo TCP/IP

**Formato**: JSON sobre TCP

**Estrutura da mensagem**:
```
[4 bytes: tamanho] [N bytes: JSON]
```

**Exemplo de mensagem de entrada**:
```json
{
  "tipo": "entrada_ok",
  "placa": "ABC1234",
  "conf": 95,
  "ts": "2025-10-09T14:30:00.000000",
  "andar": 0
}
```

### Fluxo de Comunicação

#### Entrada de Veículo
```
Servidor Térreo → Servidor Central
  Mensagem: entrada_ok
  Resposta: {status: "ok", mensagem: "Entrada autorizada"}
  
Servidor Central → Servidor Térreo
  Mensagem: atualizar_placar
  Resposta: {status: "ok"}
```

#### Saída de Veículo
```
Servidor Térreo → Servidor Central
  Mensagem: saida_ok
  Resposta: {status: "ok", valor: 6.75, tempo_minutos: 45}
  
Servidor Central → Servidor Térreo
  Mensagem: atualizar_placar
  Resposta: {status: "ok"}
```

#### Atualização de Vagas
```
Servidor Andar → Servidor Central
  Mensagem: vaga_status
  Resposta: {status: "ok"}
  
Servidor Central → Servidor Térreo
  Mensagem: atualizar_placar
  Resposta: {status: "ok"}
```

### Reconexão Automática

O `ClienteTCP` implementa reconexão automática:
```python
# Tenta 3 vezes com backoff exponencial
await cliente.enviar_com_retry(mensagem, tentativas=3)
```

---

## Protocolo MODBUS

### Formato da Mensagem

**Requisição**:
```
[Endereço Slave][Função][Dados][Matrícula (4 bytes)][CRC (2 bytes)]
```

**Resposta**:
```
[Endereço Slave][Função][Dados][Matrícula (4 bytes)][CRC (2 bytes)]
```

**Importante**: A matrícula (4 últimos dígitos) é inserida **antes** do CRC, conforme especificação do trabalho.

### Endereços dos Dispositivos

- **Câmera LPR Entrada**: `0x11`
- **Câmera LPR Saída**: `0x12`
- **Placar de Vagas**: `0x20`

### Mapa de Registros - Câmeras LPR

| Offset | Tamanho | Descrição       | Valores                           |
|--------|---------|-----------------|-----------------------------------|
| 0      | 1       | Status          | 0=Pronto, 1=Processando, 2=OK, 3=Erro |
| 1      | 1       | Trigger         | Escrever 1 para disparar          |
| 2      | 4       | Placa (8 chars) | 4 regs (2 bytes cada) - ASCII     |
| 6      | 1       | Confiança (%)   | 0-100                             |
| 7      | 1       | Erro            | Código de erro                    |

**Fluxo de captura**:
```
1. Escrever 1 em offset 1 (trigger)
2. Polling em offset 0 até Status = 2 (OK) ou 3 (Erro)
3. Ler offsets 2-5 (placa) e offset 6 (confiança)
4. Escrever 0 em offset 1 (reset trigger)
```

**Conversão de placa**:
Cada registrador de 16 bits contém 2 caracteres ASCII em big-endian:
```
Reg[0] = 0x4142 → "AB"
Reg[1] = 0x4331 → "C1"
Reg[2] = 0x4432 → "D2"
Reg[3] = 0x3300 → "3\0"
Placa completa = "ABC1D23"
```

### Mapa de Registros - Placar de Vagas

| Offset | Descrição                    |
|--------|------------------------------|
| 0      | Vagas Livres Térreo (PNE)    |
| 1      | Vagas Livres Térreo (Idoso+) |
| 2      | Vagas Livres Térreo (Comuns) |
| 3      | Vagas Livres 1º Andar (PNE)  |
| 4      | Vagas Livres 1º Andar (Idoso+) |
| 5      | Vagas Livres 1º Andar (Comuns) |
| 6      | Vagas Livres 2º Andar (PNE)  |
| 7      | Vagas Livres 2º Andar (Idoso+) |
| 8      | Vagas Livres 2º Andar (Comuns) |
| 9      | Número de carros: Térreo     |
| 10     | Número de carros: 1º Andar   |
| 11     | Número de carros: 2º Andar   |
| 12     | Flags (bit0=lotado geral, bit1=bloq. 1º, bit2=bloq. 2º) |

**Atualização**:
```
Write Multiple Registers (0x10)
Endereço: 0x20
Offset inicial: 0
Quantidade: 13 registradores
```

### Tratamento de Erros MODBUS

**Implementado**:
- Validação de CRC
- Validação de endereço e função
- Detecção de exceções MODBUS (bit 7 da função setado)
- Sistema de retentativas (até 3) com backoff exponencial
- Timeout configurável (padrão 500ms)

**Erros possíveis**:
- `ModbusException`: Erro de protocolo (CRC, endereço, função)
- `TimeoutError`: Dispositivo não respondeu
- `SerialException`: Erro na porta serial

---

## Controle de GPIO

### Varredura Multiplexada

A varredura multiplexada permite ler múltiplos sensores usando poucos pinos GPIO.

**Princípio**:
- N pinos de endereço → 2^N sensores
- 1 pino de leitura (sensor multiplexado)

**Exemplo (Térreo - 2 bits)**:
```
Endereço binário | Endereço decimal | Vaga selecionada
00               | 0                | Vaga 0
01               | 1                | Vaga 1
10               | 2                | Vaga 2
11               | 3                | Vaga 3
```

**Implementação**:
```python
def selecionar_endereco(self, endereco: int):
    for i, pino in enumerate(self.pinos_endereco):
        bit = (endereco >> i) & 1
        self.gpio.escrever(pino, HIGH if bit else LOW)
    time.sleep(0.001)  # Aguarda estabilização

def ler_vaga(self, endereco: int) -> bool:
    self.selecionar_endereco(endereco)
    return self.gpio.ler(self.pino_sensor) == HIGH
```

### Controle de Cancelas

**Máquina de Estados**:

```
       ┌─────────┐
    ┌─▶│ FECHADA │──abrir()──┐
    │  └─────────┘            │
    │                         ▼
fechaer()              ┌──────────┐
    │                  │  ABRINDO │
    │                  └──────────┘
    │                         │
    │                  sensor_abertura
    │                         │
    │                         ▼
    │  ┌────────┐        ┌────────┐
    └──│FECHANDO│◀─fechar()─│ ABERTA │
       └────────┘        └────────┘
            ▲
            │
     sensor_fechamento
```

**Segurança**:
- Timeout de 10 segundos para abertura/fechamento
- Desliga motor se timeout
- Monitora sensores de fim de curso

### Detecção de Passagem

**Dois sensores em sequência**:

```
Andar 1  [Sensor 1] ──────── [Sensor 2]  Andar 2
```

**Sequências válidas**:
- **1→2 (Subindo)**: Sensor 1 ativa, depois Sensor 2 ativa
- **2→1 (Descendo)**: Sensor 2 ativa, depois Sensor 1 ativa

**Timeout**: Se a sequência não completar em 5 segundos, reseta.

---

## Fluxos de Operação

### Fluxo de Entrada

```
1. Veículo aciona sensor de presença na entrada
   ↓
2. Servidor Térreo dispara captura na Câmera LPR (0x11)
   ↓
3. Câmera processa e retorna placa + confiança
   ↓
4. Servidor Térreo envia "entrada_ok" ao Central
   ↓
5. Central valida:
   - Estacionamento aberto?
   - Há vagas disponíveis?
   ↓
6. Se OK:
   - Central registra veículo
   - Central responde "autorizado"
   - Central atualiza placar
   ↓
7. Servidor Térreo:
   - Abre cancela de entrada
   - Aguarda veículo passar
   - Fecha cancela de entrada
```

### Fluxo de Saída

```
1. Veículo aciona sensor de presença na saída
   ↓
2. Servidor Térreo dispara captura na Câmera LPR (0x12)
   ↓
3. Câmera processa e retorna placa + confiança
   ↓
4. Servidor Térreo envia "saida_ok" ao Central
   ↓
5. Central:
   - Busca registro de entrada
   - Calcula tempo de permanência
   - Calcula valor (tempo × R$ 0,15/min)
   - Remove veículo dos ativos
   - Adiciona ao histórico
   - Atualiza placar
   ↓
6. Central responde com valor calculado
   ↓
7. Servidor Térreo:
   - Exibe recibo
   - Abre cancela de saída
   - Aguarda veículo passar
   - Fecha cancela de saída
```

### Fluxo de Passagem entre Andares

```
1. Veículo aciona Sensor 1 no 1º andar
   ↓
2. Sistema registra primeira ativação
   ↓
3. Veículo aciona Sensor 2
   ↓
4. Sistema identifica sequência (1→2 ou 2→1)
   ↓
5. Servidor Andar envia "passagem_andar" ao Central
   ↓
6. Central atualiza contadores:
   - Se subindo: -1 no andar1, +1 no andar2
   - Se descendo: +1 no andar1, -1 no andar2
   ↓
7. Central atualiza placar
```

### Fluxo de Atualização de Vagas

```
┌─────────────────────────────────────┐
│ Loop contínuo (1 segundo)           │
└─────────────────────────────────────┘
   │
   ▼
1. Servidor Andar varre todas as vagas
   ↓
2. Detecta mudanças de ocupação
   ↓
3. Envia "vaga_status" ao Central
   ↓
4. Central consolida informações
   ↓
5. Central envia "atualizar_placar" ao Térreo
   ↓
6. Servidor Térreo escreve registros MODBUS no placar (0x20)
```

---

## Instalação e Execução

### 1. Requisitos

- Raspberry Pi com Raspbian/Raspberry Pi OS
- Python 3.7+
- Acesso GPIO (executar como root ou configurar permissões)
- Porta serial USB para MODBUS (opcional, para teste)

### 2. Instalação de Dependências

```bash
cd Estacionamento_embarcados
pip3 install -r requirements.txt
```

**Dependências**:
- `RPi.GPIO`: Controle de GPIO
- `gpiozero`: Alternativa para GPIO (inclusa como fallback)
- `pyserial`: Comunicação serial
- `minimalmodbus`: Biblioteca MODBUS (não usada diretamente)
- `asyncio`: Programação assíncrona
- `aiohttp`: Servidor HTTP (não usado na versão atual)
- `python-dotenv`: Carregamento de variáveis de ambiente (opcional)

### 3. Configuração

Copie e edite o arquivo de configuração:

```bash
cp config.example config.txt
nano config.txt
```

**Principais configurações**:
- Endereços IP dos servidores
- Porta serial MODBUS
- Matrícula (4 últimos dígitos)
- Quantidade de vagas por andar
- Preço por minuto

### 4. Execução

#### Em um único computador (desenvolvimento):

```bash
# Terminal 1 - Servidor Central
./run_servidor_central.sh

# Terminal 2 - Servidor Térreo
./run_servidor_terreo.sh

# Terminal 3 - Servidor 1º Andar
./run_servidor_andar1.sh

# Terminal 4 - Servidor 2º Andar
./run_servidor_andar2.sh
```

#### Em Raspberry Pi distribuídas (produção):

**Raspberry Pi 1 (Central)**:
```bash
./run_servidor_central.sh
```

**Raspberry Pi 2 (Térreo)**:
```bash
./run_servidor_terreo.sh
```

**Raspberry Pi 3 (1º Andar)**:
```bash
./run_servidor_andar1.sh
```

**Raspberry Pi 4 (2º Andar)**:
```bash
./run_servidor_andar2.sh
```

### 5. Ordem de Inicialização

1. **Servidor Central** (primeiro)
2. **Servidor Térreo** (aguarda 1s)
3. **Servidores dos Andares** (aguardam 1s)

Os servidores distribuídos tentam reconectar automaticamente se o Central não estiver disponível.

### 6. Verificação

**Servidor Central** deve exibir:
```
Iniciando Servidor Central...
Servidor Central iniciado em 0.0.0.0:5000
```

**Servidores distribuídos** devem exibir:
```
Iniciando Servidor do Térreo...
Conexão MODBUS aberta em /dev/ttyUSB0
MODBUS inicializado com sucesso
Servidor Térreo iniciado em 0.0.0.0:5001
Tarefa de varredura de vagas iniciada
Tarefa de cancela de entrada iniciada
Tarefa de cancela de saída iniciada
Conectado ao servidor 192.168.0.100:5000
```

### 7. Interface de Monitoramento

A interface CLI do Servidor Central atualiza automaticamente e mostra:

```
================================================================================
                         SISTEMA DE ESTACIONAMENTO
================================================================================

Horário: 09/10/2025 14:30:45

STATUS GERAL:
  Estacionamento: ABERTO
  Situação: Disponível
  Veículos ativos: 3

VAGAS DISPONÍVEIS:
--------------------------------------------------------------------------------
Andar           PNE        Idoso+     Comuns     Total      Status
--------------------------------------------------------------------------------
Térreo          2/2        1/2        3/4        6/8        Disponível
1º Andar        2/2        2/2        2/4        6/8        Disponível
2º Andar        1/2        2/2        4/4        7/8        Disponível

CARROS POR ANDAR:
--------------------------------------------------------------------------------
  Térreo:   2 carros
  1º Andar: 2 carros
  2º Andar: 1 carros

================================================================================
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

---

## Tratamento de Falhas

### Reconexão TCP/IP

- Cliente tenta reconectar automaticamente se perder conexão
- Sistema de retentativas com backoff
- Servidores distribuídos continuam operando localmente

### Modo Degradado (MODBUS)

Se MODBUS falhar na inicialização:
- Sistema opera sem LPR
- Gera IDs temporários para veículos
- Cancelas funcionam normalmente
- Placar não é atualizado
- Log registra modo degradado

### Timeout de Sensores

- Cancelas têm timeout de 10s
- Se não detectar fim de curso, desliga motor e registra erro
- Sensor de passagem reseta sequência após 5s

### Baixa Confiança de LPR

Se confiança < 70%:
- **Política implementada (Extra 2)**:
  - Registra placa mesmo com baixa confiança
  - Log registra aviso
  - Permite entrada/saída normalmente
  - Operador pode reconciliar via histórico

### Veículo sem Registro de Entrada

Na saída, se veículo não tem registro:
- Permite saída (falha de segurança aberta)
- Valor de cobrança = R$ 0,00
- Registra alerta de auditoria no log
- Exibe mensagem ao operador

---

## Observações Finais

### Pontos Fortes da Implementação

1. **Arquitetura modular e escalável**
2. **Programação assíncrona eficiente** (asyncio)
3. **Tratamento robusto de erros**
4. **Reconexão automática**
5. **Interface de monitoramento intuitiva**
6. **Logs detalhados para debugging**
7. **Modo degradado funcional**
8. **Código bem documentado**

### Melhorias Possíveis

1. **Persistência de dados**: Salvar histórico em banco de dados
2. **Interface web**: Dashboard web em vez de CLI
3. **Autenticação**: Controle de acesso ao sistema
4. **Mapeamento de vagas**: Associar cada vaga a seu tipo real (PNE/Idoso/Comum)
5. **Notificações**: Alertas via email/SMS para eventos importantes
6. **Estatísticas**: Gráficos de ocupação, receita, etc.
7. **Backup**: Sistema de backup automático dos dados
8. **Monitoramento remoto**: Integração com sistemas de monitoramento

### Conformidade com os Requisitos

| Requisito | Status | Observações |
|-----------|--------|-------------|
| Servidor Central | ✅ | Interface completa, cálculo de cobranças, comandos |
| Interface de Monitoramento | ✅ | CLI com atualização em tempo real |
| Interface de Comandos | ✅ | Fechar estacionamento, bloquear andares |
| Cobrança | ✅ | R$ 0,15/min, recibo de saída |
| Vagas | ✅ | Varredura e detecção de mudanças |
| Cancelas | ✅ | Sequência correta entrada/saída |
| Passagem entre Andares | ✅ | Direção correta (1→2 sobe, 2→1 desce) |
| Câmera Entrada/Saída | ✅ | Trigger, polling, leitura de placa/confiança |
| Placar | ✅ | Escrita periódica de vagas e flags |
| Robustez Bus | ✅ | Timeout/CRC, retries, logs |
| Confiabilidade | ✅ | Reconexão TCP/IP automática |
| Qualidade/Documentação | ✅ | Código modular, README completo |
| Extra 1: Usabilidade | ✅ | Interface CLI intuitiva, scripts de inicialização |
| Extra 2: Política de Confiança | ✅ | Tratamento de confiança < 60% implementado |

---

## Licença

Este projeto foi desenvolvido para fins educacionais como parte do trabalho da disciplina de Fundamentos de Sistemas Embarcados (2025/2).

## Autor

Lucas Caldas - Matrícula: 190091606
Lucas Soares - Matrícula: 202017700

## Data

Outubro de 2025

