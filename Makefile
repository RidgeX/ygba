CC := gcc
RM := rm -f

CFLAGS := -g -O2 -std=c11 -Wall -Wextra
CFLAGS += -Icimgui -Igl3w
CFLAGS += `pkg-config --cflags freetype2 SDL2`
CFLAGS += -DCIMGUI_FREETYPE
CFLAGS += -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS
CFLAGS += -DIMGUI_IMPL_API="extern \"C\""
CFLAGS += -DIMGUI_IMPL_OPENGL_LOADER_GL3W

LIBS := -Lcimgui -lcimgui -lstdc++ -limm32
LIBS += -lopengl32
LIBS += -lmingw32 -lSDL2main -lSDL2

TARGET := gba.exe

OBJS := algorithms.o cpu.o cpu-arm.o cpu-thumb.o main.o
OBJS += gl3w/GL/gl3w.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET) $(OBJS)
