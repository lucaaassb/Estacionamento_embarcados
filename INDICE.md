# 📚 Índice de Documentação - Sistema de Estacionamento

Bem-vindo ao sistema de controle de estacionamento! Este índice te guiará pela documentação completa.

## 🚀 Para Começar

### 1️⃣ Primeiro Acesso
👉 **[RESUMO_EXECUTIVO.md](RESUMO_EXECUTIVO.md)**
- Visão geral em 5 minutos
- O que o sistema faz
- Tecnologias usadas
- Métricas do projeto

### 2️⃣ Instalação
👉 **[INSTALACAO.md](INSTALACAO.md)**
- Pré-requisitos
- Instalação passo a passo
- Como executar
- Solução de problemas

👉 **[INSTALACAO_RASPBERRY_PI.md](INSTALACAO_RASPBERRY_PI.md)** ⭐ NOVO!
- Instalação via SSH
- Execução com TMUX (4 terminais)
- Configuração para Raspberry Pi
- Script de inicialização automática

👉 **[GUIA_RAPIDO_SSH.md](GUIA_RAPIDO_SSH.md)** ⚡ REFERÊNCIA RÁPIDA
- Comandos essenciais
- Workflow típico
- Cola do TMUX

👉 **[SOLUCAO_PROBLEMAS.md](SOLUCAO_PROBLEMAS.md)** 🔧 TROUBLESHOOTING
- Soluções para erros comuns
- ModuleNotFoundError (RESOLVIDO!)
- No module named 'serial' (RESOLVIDO!)
- Problemas de permissão
- Checklist de diagnóstico

👉 **[CORRECAO_RAPIDA.md](CORRECAO_RAPIDA.md)** ⚡ CORREÇÃO URGENTE
- Comandos para copiar e colar
- Script de instalação automática
- Resolver erros rapidamente

### 3️⃣ Entendendo o Código
👉 **[EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md)**
- Explicação didática de cada módulo
- Como os componentes se conectam
- Exemplos de uso
- Fluxos completos

### 4️⃣ Arquitetura Técnica
👉 **[ARQUITETURA.md](ARQUITETURA.md)**
- Documentação técnica completa (70+ páginas)
- Especificação de protocolos
- Detalhes de implementação
- Referência completa

### 5️⃣ Especificação Original
👉 **[README.md](README.md)**
- Requisitos do trabalho
- Especificações técnicas
- Critérios de avaliação
- Dashboards de desenvolvimento

## 📂 Estrutura da Documentação

```
Estacionamento_embarcados/
│
├── INDICE.md                    ◀── Você está aqui!
├── RESUMO_EXECUTIVO.md          ◀── Comece por aqui
├── INSTALACAO.md                ◀── Como instalar
├── EXPLICACAO_CODIGO.md         ◀── Como funciona
├── ARQUITETURA.md               ◀── Documentação técnica
└── README.md                    ◀── Especificação original
```

## 🎯 Guias por Objetivo

### Quero entender o sistema rapidamente
1. [RESUMO_EXECUTIVO.md](RESUMO_EXECUTIVO.md) - Visão geral (15 min)
2. [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Como funciona (30 min)

### Quero instalar e executar
1. [INSTALACAO.md](INSTALACAO.md) - Guia de instalação (20 min)
2. Execute os scripts `run_*.sh`
3. Use a interface do Servidor Central

### Quero entender a arquitetura
1. [ARQUITETURA.md](ARQUITETURA.md) - Documentação completa (2-3 horas)
2. [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Explicações didáticas (1 hora)

### Quero modificar o código
1. [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Entenda os módulos
2. [ARQUITETURA.md](ARQUITETURA.md) - Referência técnica
3. Leia os comentários no código fonte

### Quero verificar os requisitos
1. [README.md](README.md) - Especificação original
2. [ARQUITETURA.md](ARQUITETURA.md) - Seção "Conformidade com os Requisitos"

## 📖 Conteúdo Detalhado

### RESUMO_EXECUTIVO.md
- ✅ Visão geral do sistema
- ✅ Funcionalidades principais
- ✅ Arquitetura em diagrama
- ✅ Tecnologias utilizadas
- ✅ Fluxos de operação
- ✅ Protocolo MODBUS
- ✅ Comandos da interface
- ✅ Execução rápida
- ✅ Tratamento de erros
- ✅ Diferenciais implementados
- ✅ Métricas do projeto

**Ideal para**: Apresentações, visão geral, primeira impressão

---

### INSTALACAO.md
- ✅ Pré-requisitos
- ✅ Instalação de dependências
- ✅ Configuração do sistema
- ✅ Ordem de inicialização
- ✅ Interface do Servidor Central
- ✅ Verificação de funcionamento
- ✅ Solução de problemas comuns
- ✅ Encerrando o sistema

**Ideal para**: Instalação, execução, troubleshooting

---

### EXPLICACAO_CODIGO.md
- ✅ **Módulos Comuns**
  - config.py - Configurações
  - mensagens.py - Protocolo de mensagens
  - comunicacao.py - TCP/IP
  - modbus_client.py - MODBUS
  - gpio_handler.py - GPIO
  
- ✅ **Servidor Central**
  - Lógica principal
  - Interface CLI
  - Processamento de mensagens
  
- ✅ **Servidor Térreo**
  - Controle de cancelas
  - Integração MODBUS
  - Tarefas assíncronas
  
- ✅ **Servidores dos Andares**
  - Varredura de vagas
  - Detecção de passagem
  
- ✅ **Como tudo se conecta**
  - Fluxo completo de entrada
  - Comunicação entre módulos
  - Dependências entre arquivos

**Ideal para**: Entender o código, aprender, modificar

---

### ARQUITETURA.md
- ✅ **Visão Geral**
- ✅ **Arquitetura do Sistema**
- ✅ **Estrutura de Diretórios**
- ✅ **Módulos Comuns** (detalhado)
  - Config
  - Mensagens
  - Comunicação
  - MODBUS
  - GPIO
  
- ✅ **Servidor Central** (detalhado)
  - Classes
  - Métodos
  - Lógica de negócio
  - Interface
  
- ✅ **Servidor Térreo** (detalhado)
  - GPIO
  - MODBUS
  - Cancelas
  - Tarefas
  
- ✅ **Servidores dos Andares** (detalhado)
  - Vagas
  - Passagem
  
- ✅ **Comunicação entre Servidores**
  - Protocolo TCP/IP
  - Fluxos de comunicação
  - Reconexão automática
  
- ✅ **Protocolo MODBUS**
  - Formato de mensagem
  - Mapa de registros
  - Tratamento de erros
  
- ✅ **Controle de GPIO**
  - Varredura multiplexada
  - Controle de cancelas
  - Detecção de passagem
  
- ✅ **Fluxos de Operação**
  - Entrada
  - Saída
  - Passagem entre andares
  - Atualização de vagas
  
- ✅ **Instalação e Execução**
- ✅ **Tratamento de Falhas**
- ✅ **Conformidade com Requisitos**

**Ideal para**: Referência técnica, implementação, detalhes

---

### README.md
- ✅ Objetivos do trabalho
- ✅ Arquitetura especificada
- ✅ GPIO e topologia
- ✅ Integração MODBUS
- ✅ Fluxos de operação
- ✅ Regras de negócio
- ✅ Interfaces e protocolos
- ✅ Implementação
- ✅ Dashboards
- ✅ Critérios de avaliação

**Ideal para**: Verificar requisitos, avaliar conformidade

---

## 🔍 Busca Rápida

### Procurando informações sobre...

#### Configuração
- [INSTALACAO.md](INSTALACAO.md) - Seção "Configure o sistema"
- [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "comum/config.py"

#### TCP/IP
- [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "comum/comunicacao.py"
- [ARQUITETURA.md](ARQUITETURA.md) - Seção "Comunicação entre Servidores"

#### MODBUS
- [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "comum/modbus_client.py"
- [ARQUITETURA.md](ARQUITETURA.md) - Seção "Protocolo MODBUS"
- [README.md](README.md) - Seção "Integração RS485-MODBUS"

#### GPIO
- [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "comum/gpio_handler.py"
- [ARQUITETURA.md](ARQUITETURA.md) - Seção "Controle de GPIO"
- [README.md](README.md) - Seção "GPIO e Topologia dos Andares"

#### Cancelas
- [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "ControleCancela"
- [ARQUITETURA.md](ARQUITETURA.md) - Seção "Servidor do Andar Térreo"

#### Vagas
- [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "VarredorVagas"
- [ARQUITETURA.md](ARQUITETURA.md) - Seção "Varredura Multiplexada"

#### Cobrança
- [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "Cálculo de cobrança"
- [ARQUITETURA.md](ARQUITETURA.md) - Seção "Servidor Central"

#### Interface
- [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "servidor_central/interface.py"
- [ARQUITETURA.md](ARQUITETURA.md) - Seção "Interface CLI"

#### Erros
- [INSTALACAO.md](INSTALACAO.md) - Seção "Solução de Problemas"
- [ARQUITETURA.md](ARQUITETURA.md) - Seção "Tratamento de Falhas"

## 📊 Diagramas e Figuras

### Diagrama de Arquitetura
📍 [RESUMO_EXECUTIVO.md](RESUMO_EXECUTIVO.md) - Seção "Arquitetura"
📍 [ARQUITETURA.md](ARQUITETURA.md) - Seção "Arquitetura do Sistema"

### Estrutura de Diretórios
📍 [RESUMO_EXECUTIVO.md](RESUMO_EXECUTIVO.md) - Seção "Estrutura do Código"
📍 [ARQUITETURA.md](ARQUITETURA.md) - Seção "Estrutura de Diretórios"

### Fluxos de Operação
📍 [RESUMO_EXECUTIVO.md](RESUMO_EXECUTIVO.md) - Seção "Fluxos de Operação"
📍 [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "Como Tudo se Conecta"
📍 [ARQUITETURA.md](ARQUITETURA.md) - Seção "Fluxos de Operação"

### Máquina de Estados (Cancela)
📍 [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "ControleCancela"
📍 [ARQUITETURA.md](ARQUITETURA.md) - Seção "Controle de Cancelas"

### Protocolo MODBUS
📍 [RESUMO_EXECUTIVO.md](RESUMO_EXECUTIVO.md) - Seção "Protocolo MODBUS"
📍 [ARQUITETURA.md](ARQUITETURA.md) - Seção "Protocolo MODBUS"

## 💡 Dicas de Leitura

### Para Iniciantes
1. Comece pelo [RESUMO_EXECUTIVO.md](RESUMO_EXECUTIVO.md)
2. Leia [INSTALACAO.md](INSTALACAO.md) e execute o sistema
3. Explore [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) aos poucos
4. Use [ARQUITETURA.md](ARQUITETURA.md) como referência quando necessário

### Para Experientes
1. Leia [RESUMO_EXECUTIVO.md](RESUMO_EXECUTIVO.md) rapidamente
2. Vá direto para [ARQUITETURA.md](ARQUITETURA.md)
3. Consulte [EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) para detalhes específicos
4. Use [README.md](README.md) para verificar requisitos

### Para Apresentação
1. Use [RESUMO_EXECUTIVO.md](RESUMO_EXECUTIVO.md) como base
2. Demonstre funcionamento via interface CLI
3. Mostre trechos relevantes do código
4. Consulte [ARQUITETURA.md](ARQUITETURA.md) para perguntas técnicas

## 📝 Checklist de Estudo

### Conceitos Fundamentais
- [ ] Arquitetura distribuída
- [ ] Comunicação TCP/IP
- [ ] Protocolo MODBUS
- [ ] Programação assíncrona (asyncio)
- [ ] Controle de GPIO
- [ ] Multiplexação

### Módulos do Sistema
- [ ] Módulos comuns
- [ ] Servidor Central
- [ ] Servidor Térreo
- [ ] Servidores dos Andares

### Fluxos de Operação
- [ ] Entrada de veículo
- [ ] Saída de veículo
- [ ] Passagem entre andares
- [ ] Atualização de vagas

### Protocolos
- [ ] TCP/IP (JSON)
- [ ] MODBUS RTU
- [ ] GPIO

### Execução
- [ ] Instalação
- [ ] Configuração
- [ ] Execução
- [ ] Monitoramento
- [ ] Troubleshooting

## 🎓 Recursos Adicionais

### Arquivos de Configuração
- `config.example` - Exemplo de configuração
- `requirements.txt` - Dependências Python

### Scripts de Execução
- `run_servidor_central.sh` - Inicia Servidor Central
- `run_servidor_terreo.sh` - Inicia Servidor Térreo
- `run_servidor_andar1.sh` - Inicia 1º Andar
- `run_servidor_andar2.sh` - Inicia 2º Andar

### Código Fonte
- `comum/` - Módulos compartilhados
- `servidor_central/` - Servidor Central
- `servidor_terreo/` - Servidor Térreo
- `servidor_andar/` - Servidores dos Andares

## ❓ FAQ - Perguntas Frequentes

### Onde está a explicação do protocolo MODBUS?
[ARQUITETURA.md](ARQUITETURA.md) - Seção "Protocolo MODBUS"

### Como funciona a varredura de vagas?
[EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "VarredorVagas"

### Como instalar o sistema?
[INSTALACAO.md](INSTALACAO.md)

### O que fazer se der erro de GPIO?
[INSTALACAO.md](INSTALACAO.md) - Seção "Solução de Problemas"

### Como os servidores se comunicam?
[EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Seção "Como Tudo se Conecta"

### Qual a ordem de inicialização?
[INSTALACAO.md](INSTALACAO.md) - Seção "Ordem de Inicialização"

### Como funciona o cálculo de cobrança?
[EXPLICACAO_CODIGO.md](EXPLICACAO_CODIGO.md) - Busque "Cálculo de cobrança"

### O sistema atende todos os requisitos?
[ARQUITETURA.md](ARQUITETURA.md) - Seção "Conformidade com os Requisitos"

## 📞 Informações

**Projeto**: Sistema de Controle de Estacionamento  
**Disciplina**: Fundamentos de Sistemas Embarcados (2025/2)  
**Linguagem**: Python 3  
**Plataforma**: Raspberry Pi  

---

**Boa leitura e bom aprendizado! 🚀**

