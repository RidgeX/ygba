add_executable(ygba WIN32
    backup.cpp
    backup.h
    cpu-arm.cpp
    cpu-thumb.cpp
    cpu.cpp
    cpu.h
    gpio.cpp
    gpio.h
    io.cpp
    io.h
    main.cpp
    main.h
    memory.cpp
    memory.h
)

target_link_libraries(ygba PRIVATE fmt::fmt imgui SDL2::SDL2 SDL2::SDL2main)
