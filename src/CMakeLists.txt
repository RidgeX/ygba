add_executable(ygba WIN32
    algorithms.cpp
    algorithms.h
    cpu-arm.cpp
    cpu-thumb.cpp
    cpu.cpp
    cpu.h
    io.cpp
    io.h
    main.cpp
    main.h
)

target_link_libraries(ygba PRIVATE gl3w imgui SDL2::SDL2 SDL2::SDL2main)
