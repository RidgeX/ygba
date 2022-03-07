// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include <stdint.h>
#include <cstdlib>
#include <string>

#include <fmt/core.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"
#include "imgui_memory_editor.h"

#include <SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#include "audio.h"
#include "backup.h"
#include "cpu.h"
#include "gpio.h"
#include "io.h"
#include "memory.h"
#include "system.h"
#include "video.h"

// Main code
int main(int argc, char *argv[]) {
    arm_init_lookup();
    thumb_init_lookup();

    system_read_bios_file();
    system_reset(false);

    if (argc == 2) {
        skip_bios = true;
        const std::string rom_path(argv[1]);
        system_load_rom(rom_path);
    }

    // Setup SDL
    // (Some versions of SDL before 2.0.10 appear to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled... updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char *glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char *glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);  // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags) (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("ygba", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 800, window_flags);
    if (window == nullptr) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);  // Enable vsync

    // Enable drag and drop
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    // Initalize audio
    SDL_AudioDeviceID audio_device = audio_init();

    // Initialize gamepad
    SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
    game_controller = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            game_controller = SDL_GameControllerOpen(i);
            if (game_controller != nullptr) break;
            SDL_Log("Failed to open game controller %d: %s", i, SDL_GetError());
        }
    }

    SDL_Log("OpenGL version: %s", (char *) glGetString(GL_VERSION));

    // Create textures
    glGenTextures(1, &screen_texture);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("lib/imgui/misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("lib/imgui/misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("lib/imgui/misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("lib/imgui/misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont *font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = false;
    bool show_debugger_window = true;
    bool show_memory_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                done = true;
            } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
                done = true;
            } else if (event.type == SDL_DROPFILE) {
                char *dropped_file = event.drop.file;
                const std::string rom_path(dropped_file);
                system_load_rom(rom_path);
                SDL_free(dropped_file);
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");  // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");           // Display some text (you can use format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);  // Edit bools storing our window open/close state
            ImGui::Checkbox("Debugger Window", &show_debugger_window);
            ImGui::SameLine();
            ImGui::Checkbox("Memory Window", &show_memory_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);               // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float *) &clear_color);  // Edit 3 floats representing a color

            if (ImGui::Button("Button")) {  // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            }
            ImGui::SameLine();
            ImGui::Text("%s", fmt::format("counter = {}", counter).c_str());

            ImGui::Text("%s", fmt::format("Application average {:.3f} ms/frame ({:.1f} FPS)", 1000.0f / io.Framerate, io.Framerate).c_str());
            ImGui::End();
        }

        system_process_input();

        static bool paused = false;
        if (!paused) {
            system_emulate_frame();
            if (single_step) paused = true;
        }

        glBindTexture(GL_TEXTURE_2D, screen_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, screen_pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Screen
        static int screen_scale = 3;
        ImGui::Begin("Screen");
        ImGui::SliderInt("Scale", &screen_scale, 1, 5);
        ImVec2 screen_size = ImVec2((float) SCREEN_WIDTH * screen_scale, (float) SCREEN_HEIGHT * screen_scale);
        ImGui::Image((void *) (intptr_t) screen_texture, screen_size);
        ImGui::End();

        // Debugger
        if (show_debugger_window) {
            ImGui::Begin("Debugger", &show_debugger_window);

            ImGui::Text("%s", fmt::format(" r0: {:08X}   r1: {:08X}   r2: {:08X}   r3: {:08X}", r[0], r[1], r[2], r[3]).c_str());
            ImGui::Text("%s", fmt::format(" r4: {:08X}   r5: {:08X}   r6: {:08X}   r7: {:08X}", r[4], r[5], r[6], r[7]).c_str());
            ImGui::Text("%s", fmt::format(" r8: {:08X}   r9: {:08X}  r10: {:08X}  r11: {:08X}", r[8], r[9], r[10], r[11]).c_str());
            ImGui::Text("%s", fmt::format("r12: {:08X}  r13: {:08X}  r14: {:08X}  r15: {:08X}", r[12], r[13], r[14], get_pc()).c_str());

            std::string cpsr_flag_text;
            std::string cpsr_mode_text;
            cpsr_flag_text += (cpsr & PSR_N ? "N" : "-");
            cpsr_flag_text += (cpsr & PSR_Z ? "Z" : "-");
            cpsr_flag_text += (cpsr & PSR_C ? "C" : "-");
            cpsr_flag_text += (cpsr & PSR_V ? "V" : "-");
            cpsr_flag_text += (cpsr & PSR_I ? "I" : "-");
            cpsr_flag_text += (cpsr & PSR_F ? "F" : "-");
            cpsr_flag_text += (cpsr & PSR_T ? "T" : "-");
            switch (cpsr & PSR_MODE) {
                case PSR_MODE_USR: cpsr_mode_text = "User"; break;
                case PSR_MODE_FIQ: cpsr_mode_text = "FIQ"; break;
                case PSR_MODE_IRQ: cpsr_mode_text = "IRQ"; break;
                case PSR_MODE_SVC: cpsr_mode_text = "Supervisor"; break;
                case PSR_MODE_ABT: cpsr_mode_text = "Abort"; break;
                case PSR_MODE_UND: cpsr_mode_text = "Undefined"; break;
                case PSR_MODE_SYS: cpsr_mode_text = "System"; break;
                default: cpsr_mode_text = "Illegal"; break;
            }
            ImGui::Text("%s", fmt::format("cpsr: {:08X} [{}] {}", cpsr, cpsr_flag_text, cpsr_mode_text).c_str());

            if (ImGui::Button("Run")) {
                paused = false;
                single_step = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Step")) {
                paused = false;
                single_step = true;
            }

            if (ImGui::BeginTable("disassembly", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
                uint32_t pc = get_pc();
                for (int i = 0; i < 10; i++) {
                    uint32_t address = pc + i * SIZEOF_INSTR;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", fmt::format("{:08X}", address).c_str());
                    std::string disasm_text;
                    if (FLAG_T()) {
                        uint16_t op = memory_peek_halfword(address);
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", fmt::format("{:04X}", op).c_str());
                        ImGui::TableNextColumn();
                        thumb_disasm(address, op, disasm_text);
                    } else {
                        uint32_t op = memory_peek_word(address);
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", fmt::format("{:08X}", op).c_str());
                        ImGui::TableNextColumn();
                        arm_disasm(address, op, disasm_text);
                    }
                    ImGui::Text("%s", disasm_text.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::End();
        }

        // Memory
        static MemoryEditor mem_edit;
        mem_edit.ReadFn = [](const uint8_t *data, std::size_t off) { UNUSED(data); return memory_peek_byte(off); };
        mem_edit.WriteFn = [](uint8_t *data, std::size_t off, uint8_t d) { UNUSED(data); memory_poke_byte(off, d); };
        if (show_memory_window) {
            ImGui::Begin("Memory", &show_memory_window);
            mem_edit.DrawContents(nullptr, 0x10000000);
            ImGui::End();
        }

        // Settings
        ImGui::Begin("Settings");
        ImGui::Checkbox("Has EEPROM", &has_eeprom);
        ImGui::Checkbox("Has Flash", &has_flash);
        ImGui::Checkbox("Has SRAM", &has_sram);
        ImGui::Checkbox("Has RTC", &has_rtc);
        ImGui::Checkbox("Skip BIOS", &skip_bios);

        static bool sync_to_video = true;
        ImGui::Checkbox("Sync to video", &sync_to_video);
        SDL_GL_SetSwapInterval(sync_to_video ? 1 : 0);

        static bool mute_audio = false;
        ImGui::Checkbox("Mute audio", &mute_audio);
        SDL_PauseAudioDevice(audio_device, mute_audio ? 1 : 0);

        ImGui::Text("%s", fmt::format("DMA1SAD: {:08X}", ioreg.dma[1].src_addr).c_str());
        ImGui::Text("%s", fmt::format("DMA2SAD: {:08X}", ioreg.dma[2].src_addr).c_str());
        ImGui::Text("%s", fmt::format("fifo_a_r: {}", ioreg.fifo_a_r).c_str());
        ImGui::Text("%s", fmt::format("fifo_a_w: {}", ioreg.fifo_a_w).c_str());
        ImGui::Text("%s", fmt::format("fifo_b_r: {}", ioreg.fifo_b_r).c_str());
        ImGui::Text("%s", fmt::format("fifo_b_w: {}", ioreg.fifo_b_w).c_str());

        if (ImGui::Button("Reset")) {
            system_reset(true);
        }
        if (ImGui::Button("Manual save")) {
            system_write_save_file();
        }
        ImGui::End();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int) io.DisplaySize.x, (int) io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    if (!save_path.empty()) {
        system_write_save_file();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    glDeleteTextures(1, &screen_texture);

    if (game_controller != nullptr) {
        SDL_GameControllerClose(game_controller);
    }
    if (audio_device != 0) {
        SDL_CloseAudioDevice(audio_device);
    }
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
