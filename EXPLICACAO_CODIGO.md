# Explicação Detalhada do Código

Este documento explica, de forma didática, o que cada parte do código faz e como os componentes se relacionam.

## 📚 Índice

1. [Módulos Comuns](#módulos-comuns)
2. [Servidor Central](#servidor-central)
3. [Servidor Térreo](#servidor-térreo)
4. [Servidores dos Andares](#servidores-dos-andares)
5. [Como Tudo se Conecta](#como-tudo-se-conecta)

---

## Módulos Comuns

Os módulos na pasta `comum/` são compartilhados por todos os servidores. Eles fornecem funcionalidades básicas que todos precisam usar.

### 🔧 `comum/config.py` - Gerenciador de Configurações

**O que faz**: Carrega e armazena todas as configurações do sistema (IPs, portas, preços, etc).

**Como funciona**:
```python
config = Config()  # Cria objeto de configuração
porta = config.get('servidor_central_port', 5000)  # Busca porta (padrão 5000)
```

**Principais métodos**:
- `__init__()`: Carrega configurações de arquivo ou usa padrões
- `get(chave, padrão)`: Busca uma configuração
- `set(chave, valor)`: Define uma configuração

**Por que é importante**: Centraliza todas as configurações em um único lugar, facilitando mudanças.

---

### 💬 `comum/mensagens.py` - Protocolo de Mensagens

**O que faz**: Define os tipos de mensagens que os servidores trocam entre si.

**Tipos de mensagens**:

1. **`ENTRADA_OK`**: "Um veículo acabou de entrar"
   ```python
   {
     "tipo": "entrada_ok",
     "placa": "ABC1234",
     "conf": 95,          # Confiança da leitura (%)
     "ts": "2025-10-09T14:30:00",
     "andar": 0
   }
   ```

2. **`SAIDA_OK`**: "Um veículo está saindo"
   ```python
   {
     "tipo": "saida_ok",
     "placa": "ABC1234",
     "conf": 93,
     "ts": "2025-10-09T15:45:00"
   }
   ```

3. **`VAGA_STATUS`**: "Status das vagas mudou"
   ```python
   {
     "tipo": "vaga_status",
     "andar": 1,
     "vagas_livres": {"pne": 2, "idoso": 2, "comuns": 3}
   }
   ```

**Classe `Mensagem`**: Fornece métodos estáticos para criar cada tipo de mensagem facilmente:

```python
# Em vez de criar o dict manualmente, você usa:
msg = Mensagem.criar_entrada_ok("ABC1234", confianca=95, andar=0)
```

**Por que é importante**: Padroniza a comunicação. Todos sabem exatamente qual formato esperar.

---

### 🌐 `comum/comunicacao.py` - Comunicação TCP/IP

**O que faz**: Permite que servidores conversem entre si pela rede.

#### Classe `ServidorTCP`

**Função**: Recebe conexões e mensagens de outros servidores.

**Como usar**:
```python
async def processar_mensagem(mensagem):
    # Faz algo com a mensagem
    return {"status": "ok"}

servidor = ServidorTCP("0.0.0.0", 5000, processar_mensagem)
await servidor.iniciar()  # Fica escutando na porta 5000
```

**O que acontece**:
1. Servidor fica esperando conexões
2. Quando alguém conecta, lê a mensagem
3. Chama `processar_mensagem()`
4. Envia a resposta de volta

#### Classe `ClienteTCP`

**Função**: Conecta a outros servidores e envia mensagens.

**Como usar**:
```python
cliente = ClienteTCP("192.168.0.100", 5000)
await cliente.conectar()

mensagem = {"tipo": "entrada_ok", "placa": "ABC1234"}
resposta = await cliente.enviar(mensagem, esperar_resposta=True)
```

**Recursos especiais**:
- **Reconexão automática**: Se perder conexão, tenta reconectar
- **Retentativas**: Pode tentar enviar várias vezes

**Protocolo usado**:
```
[4 bytes: tamanho da mensagem][mensagem JSON]
```

**Por que é importante**: Permite comunicação confiável entre servidores, mesmo se houver falhas temporárias.

---

### 🔌 `comum/modbus_client.py` - Cliente MODBUS

**O que faz**: Permite conversar com dispositivos MODBUS (câmeras LPR e placar).

#### Classe `ModbusClient`

**Função**: Cliente MODBUS RTU básico para comunicação serial.

**Funções MODBUS implementadas**:
1. **0x03 - Read Holding Registers**: Lê valores
2. **0x10 - Write Multiple Registers**: Escreve valores

**Como criar mensagem MODBUS**:
```python
[Endereço Slave][Função][Dados][Matrícula (4 bytes)][CRC]
```

**Exemplo de uso**:
```python
modbus = ModbusClient('/dev/ttyUSB0', 115200)
modbus.conectar()
modbus.set_matricula('1234')  # 4 últimos dígitos da matrícula

# Lê 7 registradores a partir do endereço 0 do dispositivo 0x11
valores = modbus.read_holding_registers(0x11, 0, 7)
```

**Métodos importantes**:
- `conectar()`: Abre porta serial
- `calcular_crc()`: Calcula CRC-16 MODBUS
- `read_holding_registers()`: Lê registradores
- `write_multiple_registers()`: Escreve registradores

#### Classe `CameraLPR`

**Função**: Interface de alto nível para câmeras LPR.

**Registradores da câmera**:
```
Offset 0: Status (0=Pronto, 1=Processando, 2=OK, 3=Erro)
Offset 1: Trigger (escrever 1 para capturar)
Offset 2-5: Placa (4 regs × 2 bytes = 8 caracteres)
Offset 6: Confiança (0-100%)
Offset 7: Código de erro
```

**Fluxo de captura**:
```python
camera = CameraLPR(modbus, 0x11)
resultado = camera.capturar_placa(timeout=2.0)

if resultado:
    placa, confianca = resultado
    print(f"Placa: {placa}, Confiança: {confianca}%")
```

**O que acontece internamente**:
1. Escreve `1` no registrador Trigger (offset 1)
2. Faz polling no Status (offset 0) a cada 100ms
3. Quando Status = 2 (OK):
   - Lê Placa (offsets 2-5)
   - Lê Confiança (offset 6)
4. Zera Trigger (escreve 0)

**Conversão da placa**:
```python
# Cada registrador tem 2 bytes (2 caracteres ASCII)
Reg[0] = 0x4142 → bytes [0x41, 0x42] → "AB"
Reg[1] = 0x4331 → bytes [0x43, 0x31] → "C1"
Reg[2] = 0x4432 → bytes [0x44, 0x32] → "D2"
Reg[3] = 0x3300 → bytes [0x33, 0x00] → "3\0"
Placa completa = "ABC1D23"
```

#### Classe `PlacarVagas`

**Função**: Atualiza informações no placar de vagas.

**Registradores do placar**:
```
Offset 0-2: Vagas livres Térreo (PNE, Idoso+, Comuns)
Offset 3-5: Vagas livres 1º Andar
Offset 6-8: Vagas livres 2º Andar
Offset 9-11: Número de carros por andar
Offset 12: Flags (bit0=lotado, bit1=bloq. 1º, bit2=bloq. 2º)
```

**Como usar**:
```python
placar = PlacarVagas(modbus, 0x20)

vagas_livres = {
    'terreo': {'pne': 2, 'idoso': 1, 'comuns': 3},
    'andar1': {'pne': 2, 'idoso': 2, 'comuns': 4},
    'andar2': {'pne': 1, 'idoso': 2, 'comuns': 4}
}

num_carros = {'terreo': 2, 'andar1': 2, 'andar2': 3}

placar.atualizar(vagas_livres, num_carros, 
                 lotado_geral=False, 
                 lotado_andar1=False, 
                 lotado_andar2=False)
```

**Por que é importante**: Encapsula toda a complexidade do protocolo MODBUS, oferecendo interfaces simples.

---

### 🔩 `comum/gpio_handler.py` - Controle de GPIO

**O que faz**: Gerencia sensores e atuadores da Raspberry Pi.

#### Classe `GPIOHandler`

**Função**: Operações básicas de GPIO.

**Métodos**:
```python
gpio = GPIOHandler()

# Configurar pinos
gpio.configurar_saida(23)        # Motor da cancela
gpio.configurar_entrada(7)        # Sensor

# Usar pinos
gpio.escrever(23, GPIO.HIGH)      # Liga motor
valor = gpio.ler(7)               # Lê sensor

# Interrupções
def callback(channel):
    print(f"Pino {channel} mudou!")

gpio.adicionar_interrupcao(7, callback)
```

#### Classe `VarredorVagas`

**Função**: Varre múltiplas vagas usando multiplexação.

**Conceito de multiplexação**:
```
Com N pinos de endereço, consigo ler 2^N vagas!

2 pinos → 4 vagas (00, 01, 10, 11)
3 pinos → 8 vagas (000, 001, 010, ..., 111)
```

**Como funciona**:
```
Pinos de Endereço: [A0, A1]
Sensor: S

Para ler vaga 2 (binário 10):
1. A0 = 0, A1 = 1  (seleciona vaga 2)
2. Aguarda 1ms (estabilização)
3. Lê sensor S
4. Se S = HIGH → vaga ocupada
   Se S = LOW → vaga livre
```

**Uso**:
```python
varredor = VarredorVagas(gpio, [pino_A0, pino_A1], pino_sensor)

# Varre todas as vagas
estados = varredor.varrer_todas()
# Retorna: [True, False, True, False]
#          vaga0  vaga1  vaga2  vaga3
#          ocup.  livre  ocup.  livre
```

#### Classe `ControleCancela`

**Função**: Controla uma cancela (entrada ou saída).

**Máquina de estados**:
```
FECHADA → abrir() → ABRINDO → sensor_abertura → ABERTA
   ↑                                               ↓
   └─── sensor_fechamento ← FECHANDO ← fechar() ──┘
```

**Pinos necessários**:
- `pino_motor`: Liga/desliga motor
- `pino_sensor_abertura`: Detecta quando abriu completamente
- `pino_sensor_fechamento`: Detecta quando fechou completamente

**Como usar**:
```python
cancela = ControleCancela(
    gpio,
    pino_motor=23,
    pino_sensor_abertura=7,
    pino_sensor_fechamento=1,
    nome="entrada"
)

# Abrir
if cancela.abrir():  # Retorna True se conseguiu
    print("Cancela aberta!")

# Fechar
if cancela.fechar():
    print("Cancela fechada!")

# Verificar estado
print(cancela.obter_estado())  # "ABERTA", "FECHADA", etc.
```

**Segurança**:
- Timeout de 10 segundos
- Se não detectar fim de curso, desliga motor e retorna False

#### Classe `SensorPassagem`

**Função**: Detecta quando um veículo passa entre andares.

**Como funciona**:
```
Andar 1   [Sensor 1] ─────── [Sensor 2]   Andar 2

Subindo (1→2):
1. Sensor 1 ativa
2. (aguarda até 5s)
3. Sensor 2 ativa
4. Callback "subindo" é chamado

Descendo (2→1):
1. Sensor 2 ativa
2. (aguarda até 5s)
3. Sensor 1 ativa
4. Callback "descendo" é chamado
```

**Uso**:
```python
def ao_subir():
    print("Veículo subindo!")

def ao_descer():
    print("Veículo descendo!")

sensor = SensorPassagem(
    gpio,
    pino_sensor1=22,
    pino_sensor2=11,
    callback_subindo=ao_subir,
    callback_descendo=ao_descer
)

# Os callbacks serão chamados automaticamente quando
# detectar as sequências corretas
```

**Por que é importante**: Encapsula toda a lógica de GPIO, tornando o código dos servidores muito mais simples.

---

## Servidor Central

O servidor central é o "cérebro" do sistema. Ele consolida informações de todos os andares e toma decisões.

### 📊 `servidor_central/servidor_central.py`

#### Classe `RegistroVeiculo`

**O que faz**: Armazena informações de um veículo no estacionamento.

**Atributos**:
```python
placa: str                  # "ABC1234"
timestamp_entrada: str      # "2025-10-09T14:30:00"
andar_atual: str           # "terreo", "andar1", "andar2"
confianca_entrada: int     # 95 (%)
timestamp_saida: str       # "2025-10-09T15:45:00"
confianca_saida: int       # 93 (%)
valor_pago: float          # 11.25
tempo_minutos: int         # 75
```

**Uso**:
```python
registro = RegistroVeiculo("ABC1234", "2025-10-09T14:30:00", "terreo", 95)
# Mais tarde...
registro.registrar_saida("2025-10-09T15:45:00", 93)
```

#### Classe `ServidorCentral`

**O que faz**: Gerencia todo o estacionamento.

**Estruturas de dados principais**:

1. **`veiculos_ativos`**: Dict de veículos no estacionamento
   ```python
   {
     "ABC1234": RegistroVeiculo(...),
     "XYZ5678": RegistroVeiculo(...)
   }
   ```

2. **`historico`**: Lista de veículos que já saíram
   ```python
   [
     RegistroVeiculo(...),  # Veículo 1 (saiu)
     RegistroVeiculo(...),  # Veículo 2 (saiu)
   ]
   ```

3. **`vagas_livres`**: Vagas disponíveis
   ```python
   {
     'terreo': {'pne': 2, 'idoso': 1, 'comuns': 3},
     'andar1': {'pne': 2, 'idoso': 2, 'comuns': 4},
     'andar2': {'pne': 1, 'idoso': 2, 'comuns': 4}
   }
   ```

4. **`num_carros`**: Contador de carros
   ```python
   {'terreo': 2, 'andar1': 3, 'andar2': 1}
   ```

**Fluxo de processamento de mensagens**:

```python
async def processar_mensagem(self, mensagem):
    tipo = mensagem['tipo']
    
    if tipo == "entrada_ok":
        return await self._processar_entrada(mensagem)
    elif tipo == "saida_ok":
        return await self._processar_saida(mensagem)
    # ... outros tipos
```

**Método `_processar_entrada()`**:
```python
async def _processar_entrada(self, mensagem):
    placa = mensagem['placa']
    
    # 1. Verifica se está fechado
    if self.estacionamento_fechado:
        return {"status": "erro", "motivo": "fechado"}
    
    # 2. Verifica se tem vaga
    if self._esta_lotado():
        return {"status": "erro", "motivo": "lotado"}
    
    # 3. Registra veículo
    registro = RegistroVeiculo(placa, ...)
    self.veiculos_ativos[placa] = registro
    
    # 4. Atualiza contador
    self.num_carros[andar] += 1
    
    # 5. Atualiza placar
    await self._atualizar_placar()
    
    return {"status": "ok"}
```

**Método `_processar_saida()`**:
```python
async def _processar_saida(self, mensagem):
    placa = mensagem['placa']
    
    # 1. Busca registro
    if placa not in self.veiculos_ativos:
        # Permite saída mesmo sem registro
        return {"status": "ok", "valor": 0.0}
    
    registro = self.veiculos_ativos[placa]
    
    # 2. Calcula valor
    valor, tempo = self._calcular_cobranca(
        registro.timestamp_entrada,
        mensagem['ts']
    )
    
    # 3. Atualiza registro
    registro.registrar_saida(mensagem['ts'], mensagem['conf'])
    registro.valor_pago = valor
    registro.tempo_minutos = tempo
    
    # 4. Move para histórico
    self.historico.append(registro)
    del self.veiculos_ativos[placa]
    
    # 5. Atualiza contador
    self.num_carros[registro.andar_atual] -= 1
    
    # 6. Atualiza placar
    await self._atualizar_placar()
    
    # 7. Retorna valor
    return {
        "status": "ok",
        "valor": valor,
        "tempo_minutos": tempo
    }
```

**Cálculo de cobrança**:
```python
def _calcular_cobranca(self, entrada, saida):
    # 1. Converte strings para datetime
    entrada_dt = datetime.fromisoformat(entrada)
    saida_dt = datetime.fromisoformat(saida)
    
    # 2. Calcula diferença em segundos
    tempo_segundos = (saida_dt - entrada_dt).total_seconds()
    
    # 3. Converte para minutos (arredonda pra cima)
    tempo_minutos = int(tempo_segundos / 60)
    if tempo_segundos % 60 > 0:
        tempo_minutos += 1
    
    # 4. Calcula valor
    valor = tempo_minutos * 0.15  # R$ 0,15/min
    
    return valor, tempo_minutos
```

**Exemplo**:
```
Entrada: 14:30:00
Saída:   15:45:30

Tempo: 75 minutos e 30 segundos
Arredondado: 76 minutos
Valor: 76 × R$ 0,15 = R$ 11,40
```

---

### 🖥️ `servidor_central/interface.py`

**O que faz**: Fornece interface CLI para monitoramento e controle.

#### Classe `InterfaceCLI`

**Função**: Exibe tela e processa comandos do usuário.

**Método `exibir_status()`**:
```python
def exibir_status(self):
    # 1. Limpa tela
    self.limpar_tela()
    
    # 2. Busca status do servidor
    status = self.servidor.obter_status()
    
    # 3. Exibe:
    #    - Horário
    #    - Status geral (aberto/fechado, lotado/disponível)
    #    - Vagas por andar
    #    - Carros por andar
    #    - Menu de comandos
```

**Método `processar_comando()`**:
```python
async def processar_comando(self, comando):
    if comando == '2':  # Fechar/Abrir
        status = self.servidor.obter_status()
        novo_estado = not status['estacionamento_fechado']
        await self.servidor.fechar_estacionamento(novo_estado)
    
    elif comando == '3':  # Bloquear 1º andar
        status = self.servidor.obter_status()
        novo_estado = not status['andar1_bloqueado']
        await self.servidor.bloquear_andar(1, novo_estado)
    
    # ... outros comandos
```

**Loop principal**:
```python
async def executar(self):
    while self.running:
        # Exibe tela
        self.exibir_status()
        
        # Aguarda comando (com timeout de 5s para atualização automática)
        try:
            comando = await asyncio.wait_for(input(), timeout=5.0)
            await self.processar_comando(comando)
        except asyncio.TimeoutError:
            # Atualiza tela automaticamente
            pass
```

**Por que é importante**: Permite operação e monitoramento em tempo real do estacionamento.

---

## Servidor Térreo

O servidor térreo é o mais complexo, pois controla cancelas e se comunica via MODBUS.

### 🚗 `servidor_terreo/servidor_terreo.py`

#### Classe `ServidorTerreo`

**O que faz**: Gerencia térreo (cancelas, MODBUS, vagas).

**Inicialização**:
```python
def __init__(self, config):
    # 1. Inicializa GPIO
    self.gpio = GPIOHandler()
    
    # 2. Cria varredor (2 bits = 4 vagas)
    self.varredor = VarredorVagas(...)
    
    # 3. Cria cancelas
    self.cancela_entrada = ControleCancela(...)
    self.cancela_saida = ControleCancela(...)
    
    # 4. Inicializa MODBUS
    try:
        self.modbus = ModbusClient('/dev/ttyUSB0', 115200)
        self.modbus.conectar()
        
        self.camera_entrada = CameraLPR(self.modbus, 0x11)
        self.camera_saida = CameraLPR(self.modbus, 0x12)
        self.placar = PlacarVagas(self.modbus, 0x20)
        
        self.modbus_ativo = True
    except:
        # Modo degradado (sem MODBUS)
        self.modbus_ativo = False
```

**Tarefas assíncronas**:

O servidor executa 3 tarefas em paralelo:

1. **`tarefa_varredura_vagas()`**: Varre vagas a cada 1 segundo
2. **`tarefa_cancela_entrada()`**: Gerencia entrada de veículos
3. **`tarefa_cancela_saida()`**: Gerencia saída de veículos

#### Tarefa de Entrada

```python
async def tarefa_cancela_entrada(self):
    while self.running:
        # 1. Aguarda sensor de presença
        if self.gpio.ler(sensor_presenca):
            
            # 2. Captura placa via MODBUS
            placa, confianca = None, 0
            if self.modbus_ativo:
                resultado = self.camera_entrada.capturar_placa()
                if resultado:
                    placa, confianca = resultado
            
            # 3. Se falhou, gera ID temporário
            if not placa:
                placa = f"TEMP{timestamp}"
                confianca = 0
            
            # 4. Comunica com Central
            mensagem = Mensagem.criar_entrada_ok(placa, confianca, 0)
            resposta = await self.cliente_central.enviar(
                mensagem, 
                esperar_resposta=True
            )
            
            # 5. Se autorizado
            if resposta['status'] == 'ok':
                # Abre cancela
                self.cancela_entrada.abrir()
                
                # Aguarda veículo passar
                while self.gpio.ler(sensor_presenca):
                    await asyncio.sleep(0.5)
                await asyncio.sleep(2)
                
                # Fecha cancela
                self.cancela_entrada.fechar()
```

**Fluxo visual**:
```
Veículo chega
    ↓
Sensor detecta
    ↓
Captura placa (MODBUS)
    ↓
Envia ao Central
    ↓
Central autoriza?
    ├─ Sim → Abre cancela
    └─ Não → Mantém fechada
    ↓
Aguarda passar
    ↓
Fecha cancela
```

#### Tarefa de Saída

```python
async def tarefa_cancela_saida(self):
    while self.running:
        # 1. Aguarda sensor
        if self.gpio.ler(sensor_presenca):
            
            # 2. Captura placa
            placa, confianca = None, 0
            if self.modbus_ativo:
                resultado = self.camera_saida.capturar_placa()
                if resultado:
                    placa, confianca = resultado
            
            # 3. Se falhou, permite saída
            if not placa:
                placa = "DESCONHECIDO"
            
            # 4. Comunica com Central
            mensagem = Mensagem.criar_saida_ok(placa, confianca)
            resposta = await self.cliente_central.enviar(
                mensagem,
                esperar_resposta=True
            )
            
            # 5. Exibe recibo
            if resposta['status'] == 'ok':
                valor = resposta['valor']
                tempo = resposta['tempo_minutos']
                
                print(f"\n{'='*50}")
                print(f"RECIBO DE PAGAMENTO")
                print(f"{'='*50}")
                print(f"Placa: {placa}")
                print(f"Tempo: {tempo} minutos")
                print(f"Valor: R$ {valor:.2f}")
                print(f"{'='*50}\n")
                
                # Abre cancela
                self.cancela_saida.abrir()
                
                # Aguarda passar
                while self.gpio.ler(sensor_presenca):
                    await asyncio.sleep(0.5)
                await asyncio.sleep(2)
                
                # Fecha cancela
                self.cancela_saida.fechar()
```

---

## Servidores dos Andares

Os servidores dos andares (1º e 2º) são mais simples: apenas monitoram vagas e detectam passagens.

### 🏢 `servidor_andar/servidor_andar.py`

#### Classe `ServidorAndar`

**O que faz**: Gerencia um andar (vagas + passagem).

**Construtor**:
```python
def __init__(self, config, numero_andar):
    self.numero_andar = numero_andar  # 1 ou 2
    
    # Seleciona configuração GPIO apropriada
    if numero_andar == 1:
        gpio_config = GPIO_ANDAR1
    else:
        gpio_config = GPIO_ANDAR2
    
    # Inicializa GPIO
    self.gpio = GPIOHandler()
    
    # Varredor (3 bits = 8 vagas)
    self.varredor = VarredorVagas(
        self.gpio,
        [gpio_config['endereco_01'], 
         gpio_config['endereco_02'], 
         gpio_config['endereco_03']],
        gpio_config['sensor_vaga']
    )
    
    # Sensor de passagem
    self.sensor_passagem = SensorPassagem(
        self.gpio,
        gpio_config['sensor_passagem_1'],
        gpio_config['sensor_passagem_2'],
        self._callback_subindo,
        self._callback_descendo
    )
```

**Callbacks de passagem**:
```python
def _callback_subindo(self):
    # Só o 1º andar detecta subida (1→2)
    if self.numero_andar == 1:
        asyncio.create_task(self._enviar_passagem("subindo"))

def _callback_descendo(self):
    # Só o 1º andar detecta descida (2→1)
    if self.numero_andar == 1:
        asyncio.create_task(self._enviar_passagem("descendo"))

async def _enviar_passagem(self, direcao):
    mensagem = Mensagem.criar_passagem_andar(direcao)
    await self.cliente_central.enviar(mensagem)
```

**Por que só o 1º andar?**
- Os sensores de passagem ficam entre o 1º e 2º andares
- Não faz sentido duplicar a detecção
- O 1º andar detecta e notifica o Central
- O Central atualiza os contadores de ambos os andares

**Tarefa de varredura**:
```python
async def tarefa_varredura_vagas(self):
    while self.running:
        # 1. Varre vagas
        estados = self.varredor.varrer_todas()
        
        # 2. Detecta mudanças
        mudancas = []
        for i, ocupada in enumerate(estados):
            if ocupada != self.vagas_ocupadas[i]:
                mudancas.append((i, ocupada))
                self.vagas_ocupadas[i] = ocupada
        
        # 3. Se houve mudança
        if mudancas:
            # Calcula vagas livres
            total_livres = sum(1 for ocupada in estados if not ocupada)
            
            # Distribui por tipo (simplificado)
            # Em produção, cada vaga teria tipo definido
            
            # Envia ao Central
            mensagem = Mensagem.criar_vaga_status(
                self.numero_andar,
                self.vagas_livres
            )
            await self.cliente_central.enviar(mensagem)
        
        # Aguarda 1 segundo
        await asyncio.sleep(1.0)
```

---

## Como Tudo se Conecta

### Fluxo Completo: Entrada de Veículo

```
1. [SENSOR] Veículo aciona sensor de presença
   ↓
2. [TERREO] Detecta via GPIO.ler(sensor_presenca)
   ↓
3. [TERREO] Dispara captura na câmera
   camera_entrada.capturar_placa()
   ↓
4. [MODBUS] Câmera processa e retorna placa
   ↓
5. [TERREO] Cria mensagem
   msg = Mensagem.criar_entrada_ok("ABC1234", 95, 0)
   ↓
6. [TCP/IP] Envia ao Central
   cliente_central.enviar(msg, esperar_resposta=True)
   ↓
7. [CENTRAL] Recebe via ServidorTCP
   processar_mensagem(msg)
   ↓
8. [CENTRAL] Valida
   - Estacionamento aberto?
   - Tem vaga?
   ↓
9. [CENTRAL] Registra veículo
   self.veiculos_ativos["ABC1234"] = RegistroVeiculo(...)
   ↓
10. [CENTRAL] Atualiza placar
    _atualizar_placar()
    ↓
11. [TCP/IP] Envia comando ao Térreo
    msg = Mensagem.criar_atualizar_placar(...)
    cliente_terreo.enviar(msg)
    ↓
12. [TERREO] Recebe comando
    processar_mensagem(msg)
    ↓
13. [MODBUS] Atualiza placar
    placar.atualizar(vagas_livres, num_carros, ...)
    ↓
14. [CENTRAL] Responde ao Térreo
    {"status": "ok", "mensagem": "Entrada autorizada"}
    ↓
15. [TERREO] Recebe autorização
    ↓
16. [TERREO] Abre cancela
    cancela_entrada.abrir()
    ↓
17. [GPIO] Liga motor, aguarda sensor de abertura
    ↓
18. [TERREO] Aguarda veículo passar
    while gpio.ler(sensor_presenca): sleep(0.5)
    ↓
19. [TERREO] Fecha cancela
    cancela_entrada.fechar()
    ↓
20. [GPIO] Liga motor, aguarda sensor de fechamento
    ↓
FIM
```

### Comunicação entre Módulos

```
┌─────────────────────────────────────────────────────────┐
│                    SERVIDOR CENTRAL                     │
│                                                         │
│  veiculos_ativos = {}                                  │
│  historico = []                                        │
│  vagas_livres = {...}                                  │
│                                                         │
│  processar_mensagem()                                  │
│    ├─ _processar_entrada()                            │
│    ├─ _processar_saida()                              │
│    ├─ _calcular_cobranca()                            │
│    └─ _atualizar_placar()                             │
└──────────────┬────────────────────────────────────────┘
               │
               │ TCP/IP (ClienteTCP / ServidorTCP)
               │
    ┌──────────┴──────────┬───────────────┐
    │                     │               │
┌───▼────────────┐  ┌─────▼────────┐  ┌──▼───────────┐
│ SERVIDOR TÉRREO│  │SERVIDOR ANDAR│  │SERVIDOR ANDAR│
│                │  │      1º      │  │      2º      │
│ GPIO:          │  │              │  │              │
│  ├─ Varredor   │  │ GPIO:        │  │ GPIO:        │
│  ├─ Cancelas   │  │  ├─ Varredor │  │  ├─ Varredor │
│  └─ Sensores   │  │  └─ Passagem │  │  └─ Passagem │
│                │  │              │  │              │
│ MODBUS:        │  └──────────────┘  └──────────────┘
│  ├─ Câmeras    │
│  └─ Placar     │
└────────────────┘
```

### Dependências entre Arquivos

```
run_servidor_central.sh
    └─> servidor_central/interface.py
            └─> servidor_central/servidor_central.py
                    ├─> comum/config.py
                    ├─> comum/mensagens.py
                    └─> comum/comunicacao.py

run_servidor_terreo.sh
    └─> servidor_terreo/servidor_terreo.py
            ├─> comum/config.py
            ├─> comum/mensagens.py
            ├─> comum/comunicacao.py
            ├─> comum/modbus_client.py
            └─> comum/gpio_handler.py

run_servidor_andar1.sh
    └─> servidor_andar/servidor_andar.py (arg: 1)
            ├─> comum/config.py
            ├─> comum/mensagens.py
            ├─> comum/comunicacao.py
            └─> comum/gpio_handler.py
```

---

## Resumo: O que cada arquivo faz

| Arquivo | Função Principal | Classes/Funções Chave |
|---------|------------------|----------------------|
| `comum/config.py` | Gerencia configurações | `Config` |
| `comum/mensagens.py` | Define protocolo de mensagens | `Mensagem`, `TipoMensagem` |
| `comum/comunicacao.py` | Comunicação TCP/IP | `ServidorTCP`, `ClienteTCP` |
| `comum/modbus_client.py` | Comunicação MODBUS | `ModbusClient`, `CameraLPR`, `PlacarVagas` |
| `comum/gpio_handler.py` | Controle de GPIO | `GPIOHandler`, `VarredorVagas`, `ControleCancela`, `SensorPassagem` |
| `servidor_central/servidor_central.py` | Lógica central | `ServidorCentral`, `RegistroVeiculo` |
| `servidor_central/interface.py` | Interface do usuário | `InterfaceCLI` |
| `servidor_terreo/servidor_terreo.py` | Controle do térreo | `ServidorTerreo` |
| `servidor_andar/servidor_andar.py` | Controle dos andares | `ServidorAndar` |

---

**Próximos passos**: Leia `ARQUITETURA.md` para detalhes técnicos completos!

