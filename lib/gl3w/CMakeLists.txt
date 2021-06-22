add_library(gl3w OBJECT
    gl3w.c
    GL/gl3w.h
    GL/glcorearb.h
    KHR/khrplatform.h
)

if(UNIX)
    target_link_libraries(gl3w PRIVATE dl GL)
elseif(WIN32)
    target_link_libraries(gl3w PRIVATE opengl32)
endif()

target_include_directories(gl3w PUBLIC .)
