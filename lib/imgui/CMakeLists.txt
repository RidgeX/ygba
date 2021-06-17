add_library(imgui OBJECT
    imconfig.h
    imgui.cpp
    imgui.h
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_internal.h
    imgui_memory_editor.h
    imgui_tables.cpp
    imgui_widgets.cpp
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
    backends/imgui_impl_opengl3.cpp
    backends/imgui_impl_opengl3.h
    backends/imgui_impl_sdl.cpp
    backends/imgui_impl_sdl.h
    misc/freetype/imgui_freetype.cpp
    misc/freetype/imgui_freetype.h
)

target_link_libraries(imgui PRIVATE Freetype::Freetype gl3w SDL2::SDL2)
target_include_directories(imgui PUBLIC . backends)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_IMPL_OPENGL_LOADER_GL3W)
