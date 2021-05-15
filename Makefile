CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2 -DDEBUG
LIBS = -lmingw32 -lSDL2main -lSDL2

gba: main.o cpu.o cpu-arm.o cpu-thumb.o
	gcc $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	gcc $(CFLAGS) -c $<

clean:
	rm -f gba.exe *.o
