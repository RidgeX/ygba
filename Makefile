CC := gcc
CXX := g++
RM := rm -f

CFLAGS := -g -O2 -std=c11 -Wall -Wextra

CXXFLAGS := -g -O2 -std=c++11 -Wall -Wextra

CPPFLAGS := -Ilib/imgui -Ilib/imgui/backends -Ilib/gl3w
CPPFLAGS += `pkg-config --cflags freetype2 SDL2`
CPPFLAGS += -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS
CPPFLAGS += -DIMGUI_IMPL_OPENGL_LOADER_GL3W

LIBS := -lfreetype
LIBS += -limm32 -lopengl32
LIBS += -lmingw32 -lSDL2main -lSDL2 -mwindows

TARGET := gba.exe

OBJS := src/algorithms.o src/cpu.o src/cpu-arm.o src/cpu-thumb.o src/main.o
OBJS += lib/gl3w/GL/gl3w.o
OBJS += lib/imgui/imgui.o lib/imgui/imgui_demo.o lib/imgui/imgui_draw.o lib/imgui/imgui_tables.o lib/imgui/imgui_widgets.o
OBJS += lib/imgui/backends/imgui_impl_opengl3.o lib/imgui/backends/imgui_impl_sdl.o
OBJS += lib/imgui/misc/freetype/imgui_freetype.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET) $(OBJS)
