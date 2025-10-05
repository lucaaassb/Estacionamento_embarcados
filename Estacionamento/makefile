
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

LINKFLAGS := -lbcm2835 -pthread $(MODBUS_LIBS)
SRCFILES := $(wildcard src/*.c)

all: dirs $(SRCFILES:src/%.c=obj/%.o)
	$(CC) $(CFLAGS) obj/*.o -o bin/main $(LINKFLAGS)

obj/%.o: src/%.c
	$(CC) $(CFLAGS) $(MODBUS_CFLAGS) -c $< -o $@ -I./inc
	
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


.PHONY: clean
clean:
	rm -rf obj bin
	mkdir -p obj bin
	touch obj/.gitkeep
	touch bin/.gitkeep