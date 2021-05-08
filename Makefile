CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
LIBS = -lmingw32 -lSDL2main -lSDL2

gba: gba.c
	gcc $(CFLAGS) -o gba gba.c $(LIBS)

clean:
	rm gba.exe
