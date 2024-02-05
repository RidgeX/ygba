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
    backends/imgui_impl_opengl3_loader.h
    backends/imgui_impl_sdl2.cpp
    backends/imgui_impl_sdl2.h
    misc/cpp/imgui_stdlib.cpp
    misc/cpp/imgui_stdlib.h
    misc/freetype/imgui_freetype.cpp
    misc/freetype/imgui_freetype.h
)

if(UNIX)
    target_link_libraries(imgui PRIVATE dl GL)
elseif(WIN32)
    target_link_libraries(imgui PRIVATE opengl32)
endif()

target_link_libraries(imgui PRIVATE Freetype::Freetype SDL2::SDL2)
target_include_directories(imgui PUBLIC . backends)
