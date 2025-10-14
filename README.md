# Sistema de Controle de Estacionamento - Sistemas Embarcados

## Autores
- **Lucas Soares Barros** - Matrícula: 202017700
- **Lucas Caldas Barbosa de Souza** - Matrícula: 190091606

## Descrição do Projeto

Sistema distribuído para controle e monitoramento de um estacionamento de 3 andares, desenvolvido para a disciplina de **Fundamentos de Sistemas Embarcados (2025/2)**. O sistema inclui:

- Controle de entrada/saída de veículos com cancelas e sensores
- Leitura de ocupação de vagas por endereço (multiplexação GPIO)
- Cobrança por tempo de permanência (R$ 0,15 por minuto)
- Integração serial RS485-MODBUS com câmeras LPR e placar de vagas
- Comunicação TCP/IP entre servidores distribuídos

## Arquitetura do Sistema

O sistema é composto por:
- **Servidor Central**: Consolida dados, calcula valores e gerencia interface
- **Servidor Térreo**: Controla cancelas, sensores de vaga e dispositivos MODBUS
- **Servidor 1º Andar**: Monitora vagas e passagem entre andares
- **Servidor 2º Andar**: Monitora vagas e passagem entre andares

## Pré-requisitos

- Raspberry Pi com sistema operacional Linux
- Compilador GCC
- Biblioteca BCM2835 para controle GPIO
- Acesso SSH à Raspberry Pi

## Instalação e Execução

### Passo 1: Conectar via SSH na Raspberry Pi

```bash
ssh lucasbarros@164.41.98.2 -p 15006
```

### Passo 2: Copiar arquivos para a Raspberry Pi

```bash
scp -P 15006 -r src inc makefile lucasbarros@164.41.98.2:~/Estacionamento_SistemasEmbarcados/Estacionamento/
```

**Nota**: Caso ocorra erro, criar a pasta no diretório da Raspberry Pi:
```bash
mkdir -p ~/Estacionamento_SistemasEmbarcados/Estacionamento/
```

### Passo 3: Compilar o projeto

Após a cópia dos arquivos, acesse o diretório do projeto e compile:

```bash
cd ~/Estacionamento_SistemasEmbarcados/Estacionamento/
make clean && make
```

### Passo 4: Executar o sistema

#### Terminal Central (Servidor Central)
No terminal principal, execute:
```bash
bin/main c
```

#### Terminais Distribuídos
Abra **3 terminais adicionais** e conecte via SSH em cada um:

**Terminal 1 - Servidor Térreo:**
```bash
ssh lucasbarros@164.41.98.2 -p 15006
cd ~/Estacionamento_SistemasEmbarcados/Estacionamento/
bin/main t
```

**Terminal 2 - Servidor 1º Andar:**
```bash
ssh lucasbarros@164.41.98.2 -p 15006
cd ~/Estacionamento_SistemasEmbarcados/Estacionamento/
bin/main u
```

**Terminal 3 - Servidor 2º Andar:**
```bash
ssh lucasbarros@164.41.98.2 -p 15006
cd ~/Estacionamento_SistemasEmbarcados/Estacionamento/
bin/main d
```

## Estrutura do Projeto

```
Estacionamento/
├── bin/                    # Executáveis compilados
│   └── main               # Executável principal
├── src/                   # Código fonte
│   ├── main.c            # Arquivo principal
│   ├── servidorCentral.c # Servidor central
│   ├── terreo.c          # Servidor térreo
│   ├── 1Andar.c          # Servidor 1º andar
│   ├── 2Andar.c          # Servidor 2º andar
│   ├── modbus.c          # Comunicação MODBUS
│   └── lpr_terreo.c      # Leitura de placas
├── inc/                   # Cabeçalhos
│   ├── central.h
│   ├── terreo.h
│   ├── andar1.h
│   ├── andar2.h
│   ├── modbus.h
│   └── lpr_terreo.h
├── obj/                   # Objetos compilados
├── makefile              # Arquivo de compilação
└── config.env            # Configurações
```

## Comandos do Makefile

- `make` ou `make all`: Compila todo o projeto
- `make clean`: Limpa arquivos compilados
- `make clean && make`: Limpa e recompila
- `make terreo`: Executa servidor térreo
- `make central`: Executa servidor central
- `make andar1`: Executa servidor 1º andar
- `make andar2`: Executa servidor 2º andar

## Funcionalidades

### Servidor Central
- Interface de monitoramento em tempo real
- Cálculo de valores por tempo de permanência
- Comandos de controle (fechar estacionamento, bloquear andares)
- Consolidação de dados de todos os andares

### Servidor Térreo
- Controle de cancelas de entrada e saída
- Leitura de sensores de vaga (endereços 0-7)
- Comunicação MODBUS com câmeras LPR
- Atualização do placar de vagas

### Servidores de Andares
- Monitoramento de vagas por endereço
- Detecção de passagem entre andares
- Comunicação TCP/IP com servidor central

## Integração MODBUS

O sistema utiliza comunicação RS485-MODBUS RTU para:
- **Câmera LPR Entrada** (endereço 0x11)
- **Câmera LPR Saída** (endereço 0x12)
- **Placar de Vagas** (endereço 0x20)

## Configuração GPIO

### Andar Térreo
- Endereços de vaga: GPIO 17, 18
- Sensor de vaga: GPIO 8
- Cancelas entrada/saída: GPIO 7, 1, 12, 25
- Motores cancelas: GPIO 23, 24

### 1º Andar
- Endereços de vaga: GPIO 16, 20, 21
- Sensor de vaga: GPIO 27
- Sensores de passagem: GPIO 22, 11

### 2º Andar
- Endereços de vaga: GPIO 0, 5, 6
- Sensor de vaga: GPIO 13
- Sensores de passagem: GPIO 19, 26