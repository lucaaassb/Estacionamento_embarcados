
# Executavel
BINFOLDER := bin/
# .hpp
INCFOLDER := inc/
# .cpp
SRCFOLDER := src/
# .o
OBJFOLDER := obj/
CC := gcc
CFLAGS := -Wall -Wextra -std=c99

# Detecta flags do libmodbus via pkg-config (se disponível)
MODBUS_CFLAGS := $(shell pkg-config --cflags libmodbus 2>/dev/null)
MODBUS_LIBS := $(shell pkg-config --libs libmodbus 2>/dev/null)

# Fallback caso pkg-config não esteja disponível
ifeq ($(strip $(MODBUS_LIBS)),)
  MODBUS_LIBS := -lmodbus
endif

ifdef EXTRA_CFLAGS
  ifneq (,$(findstring -DNO_MODBUS,$(EXTRA_CFLAGS)))
    MODBUS_LIBS :=
  endif
endif

LINKFLAGS := -lbcm2835 -pthread $(MODBUS_LIBS) -lcurl -ljson-c
SRCFILES := $(wildcard src/*.c)

all: dirs $(SRCFILES:src/%.c=obj/%.o)
	$(CC) $(CFLAGS) obj/*.o -o bin/main $(LINKFLAGS)

obj/%.o: src/%.c
	$(CC) $(CFLAGS) $(MODBUS_CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@ -I./inc
	
dirs:
	mkdir -p obj bin

terreo: 
	bin/main t

central:
	bin/main c

andar1:
	bin/main u

andar2:
	bin/main d

test: all
	@echo "Executando teste completo do sistema..."
	@chmod +x teste_sistema_completo.sh
	@./teste_sistema_completo.sh

install-deps:
	@echo "Instalando dependências do sistema..."
	@sudo apt-get update  
	@sudo apt-get install -y libbcm2835-dev libmodbus-dev build-essential libcurl4-openssl-dev libjson-c-dev

config-thingsboard:
	@echo "Configurando tokens do ThingsBoard..."
	@chmod +x configurar_thingsboard.sh
	@./configurar_thingsboard.sh

setup: install-deps dirs
	@echo "Sistema preparado para compilação"
	@echo "Execute 'make config-thingsboard' para configurar o dashboard"

.PHONY: clean clean-all test install-deps setup
clean:
	rm -rf obj bin
	mkdir -p obj bin logs
	touch obj/.gitkeep
	touch bin/.gitkeep

clean-all: clean
	rm -rf logs/*.log