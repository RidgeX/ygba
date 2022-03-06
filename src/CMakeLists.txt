add_executable(ygba WIN32
    audio.cpp
    audio.h
    backup.cpp
    backup.h
    cpu-arm.cpp
    cpu-thumb.cpp
    cpu.cpp
    cpu.h
    dma.cpp
    dma.h
    gpio.cpp
    gpio.h
    io.cpp
    io.h
    main.cpp
    memory.cpp
    memory.h
    system.cpp
    system.h
    timer.cpp
    timer.h
    video.cpp
    video.h
)

target_link_libraries(ygba PRIVATE fmt::fmt imgui SDL2::SDL2 SDL2::SDL2main)
