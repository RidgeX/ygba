add_library(gl3w OBJECT
    GL/gl3w.c
    GL/gl3w.h
    GL/glcorearb.h
)

if(WIN32)
    target_link_libraries(gl3w PRIVATE opengl32)
endif()

target_include_directories(gl3w PUBLIC .)
