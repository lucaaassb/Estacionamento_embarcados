
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
LINKFLAGS := -lbcm2835 -pthread -lmodbus
SRCFILES := $(wildcard src/*.c)

all: dirs $(SRCFILES:src/%.c=obj/%.o)
	$(CC) $(CFLAGS) obj/*.o -o bin/main $(LINKFLAGS)

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@ -I./inc $(LINKFLAGS)
	
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