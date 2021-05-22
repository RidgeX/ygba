CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -g -O2 -shared-libgcc
LIBS = -lmingw32 -lSDL2main -lSDL2

gba: obj/algorithms.o obj/cpu.o obj/cpu-arm.o obj/cpu-thumb.o obj/main.o
	gcc $(CFLAGS) -o $@ $^ $(LIBS)

obj/%.o: %.c
	gcc $(CFLAGS) -o $@ -c $<

clean:
	rm -f gba.exe obj/*.o
