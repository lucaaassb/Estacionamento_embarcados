
# Executavel
BINFOLDER := bin/
# .hpp
INCFOLDER := inc/
# .cpp
SRCFOLDER := src/
# .o
OBJFOLDER := obj/
CC := gcc
CFLAGS := 
LINKFLAGS := -lbcm2835 -pthread
SRCFILES := src/main.c src/1Andar.c src/2Andar.c src/servidorCentral.c src/terreo.c src/modbus.c src/lpr_terreo.c

all: $(SRCFILES:src/%.c=obj/%.o)
	mkdir -p bin
	$(CC) $(CFLAGS) obj/*.o -o bin/main $(LINKFLAGS)

obj/%.o: src/%.c
	mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@ -I./inc $(LINKFLAGS)
	
terreo: 
	bin/main t

central:
	bin/main c

andar1:
	bin/main u

andar2:
	bin/main d

teste_manual: obj/terreo.o
	mkdir -p bin
	$(CC) $(CFLAGS) obj/terreo.o teste_manual.c -o bin/teste_manual $(LINKFLAGS) -I./inc

.PHONY: clean
clean:
	mkdir -p obj bin
	rm -rf obj/*
	rm -rf bin/*
	touch obj/.gitkeep
	touch bin/.gitkeep