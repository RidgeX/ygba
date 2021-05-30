AR := ar
CXX := g++
RM := rm -f

CXXFLAGS := -g -O2 -std=c++11 -Wall -Wextra
CXXFLAGS += -Iimgui -I../gl3w
CXXFLAGS += `pkg-config --cflags freetype2 SDL2`
CXXFLAGS += -DCIMGUI_FREETYPE
CXXFLAGS += -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS
CXXFLAGS += -DIMGUI_IMPL_API="extern \"C\""
CXXFLAGS += -DIMGUI_IMPL_OPENGL_LOADER_GL3W

TARGET := libcimgui.a

OBJS := cimgui.o
OBJS += imgui/imgui.o imgui/imgui_demo.o imgui/imgui_draw.o imgui/imgui_tables.o imgui/imgui_widgets.o
OBJS += imgui/backends/imgui_impl_opengl3.o imgui/backends/imgui_impl_sdl.o
OBJS += imgui/misc/freetype/imgui_freetype.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET) $(OBJS)
