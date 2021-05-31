CXX := g++
RM := rm -f

CXXFLAGS := -g -O2 -std=c++11 -Wall -Wextra
CXXFLAGS += -Icimgui -Igl3w
CXXFLAGS += `pkg-config --cflags freetype2 SDL2`
CXXFLAGS += -DCIMGUI_FREETYPE
CXXFLAGS += -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS
CXXFLAGS += -DIMGUI_IMPL_API="extern \"C\""
CXXFLAGS += -DIMGUI_IMPL_OPENGL_LOADER_GL3W

LIBS := -Lcimgui -lcimgui -lstdc++ -limm32
LIBS += -lopengl32
LIBS += -lmingw32 -lSDL2main -lSDL2

TARGET := gba.exe

OBJS := algorithms.o cpu.o cpu-arm.o cpu-thumb.o main.o
OBJS += gl3w/GL/gl3w.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LIBS)

%.o: %.c
	$(CXX) $(CXXFLAGS) -x c -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET) $(OBJS)
