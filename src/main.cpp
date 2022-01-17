// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "main.h"

#include <stdint.h>
#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <functional>
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
#include "video.h"

bool single_step;
uint32_t ppu_cycles;
bool halted;
int active_dma;
bool skip_bios;
std::string save_path;
uint32_t idle_loop_address;
uint16_t idle_loop_last_irq;

void gba_check_keypad_interrupt() {
    if (BIT(ioreg.keycnt.w, 14)) {
        uint16_t held = ~ioreg.keyinput.w & 0x3ff;
        uint16_t mask = ioreg.keycnt.w & 0x3ff;
        if (BIT(ioreg.keycnt.w, 15)) {
            if ((held & mask) == mask) {  // All keys held
                ioreg.irq.w |= INT_BUTTON;
            }
        } else {
            if (held & mask) {  // Any key held
                ioreg.irq.w |= INT_BUTTON;
            }
        }
    }
}

void gba_timer_reset(int i) {
    ioreg.timer[i].counter.w = ioreg.timer[i].reload.w;
    ioreg.timer[i].elapsed = 0;
}

uint32_t gba_open_bus() {
    if (FLAG_T()) {
        return thumb_pipeline[1] | thumb_pipeline[1] << 16;
    } else {
        return arm_pipeline[1];
    }
}

static bool game_rom_contains(const std::string &s) {
    uint8_t *begin = game_rom;
    uint8_t *end = begin + game_rom_size;
    const auto it = std::search(begin, end, std::boyer_moore_searcher(s.begin(), s.end()));
    return it != end;
}

static void gba_detect_cartridge_features() {
    has_eeprom = false;
    has_flash = false;
    has_sram = false;
    has_rtc = false;
    idle_loop_address = 0;

    if (game_rom_contains("EEPROM_V")) {
        has_eeprom = true;
    }
    if (game_rom_contains("FLASH_V") || game_rom_contains("FLASH512_V")) {
        has_flash = true;
        flash_manufacturer = MANUFACTURER_PANASONIC;
        flash_device = DEVICE_MN63F805MNP;
    }
    if (game_rom_contains("FLASH1M_V")) {
        has_flash = true;
        flash_manufacturer = MANUFACTURER_SANYO;
        flash_device = DEVICE_LE26FV10N1TS;
    }
    if (game_rom_contains("SRAM_V") || game_rom_contains("SRAM_F_V")) {
        has_sram = true;
    }
    if (game_rom_contains("SIIRTC_V")) {
        has_rtc = true;
    }

    std::string game_title((char *) &game_rom[0xa0], 12);
    if (const auto it = std::find(game_title.begin(), game_title.end(), '\0'); it != game_title.end()) {
        game_title.erase(it, game_title.end());
    }
    std::string game_code((char *) &game_rom[0xac], 4);
    if (const auto it = std::find(game_code.begin(), game_code.end(), '\0'); it != game_code.end()) {
        game_code.erase(it, game_code.end());
    }
    uint8_t game_version = game_rom[0xbc];

    // Advance Wars (USA)
    if (game_title == "ADVANCEWARS" && game_code == "AWRE" && game_version == 0) idle_loop_address = 0x80387ec;
    // Advance Wars (USA) (Rev 1)
    if (game_title == "ADVANCEWARS" && game_code == "AWRE" && game_version == 1) idle_loop_address = 0x8038818;
    // Advance Wars 2 - Black Hole Rising (USA, Australia)
    if (game_title == "ADVANCEWARS2" && game_code == "AW2E" && game_version == 0) idle_loop_address = 0x8036e0c;
    // Pokemon - Emerald Version (USA, Europe)
    if (game_title == "POKEMON EMER" && game_code == "BPEE" && game_version == 0) idle_loop_address = 0x80008c6;
    // Pokemon - FireRed Version (USA)
    if (game_title == "POKEMON FIRE" && game_code == "BPRE" && game_version == 0) idle_loop_address = 0x80008aa;
    // Pokemon - FireRed Version (USA, Europe) (Rev 1)
    if (game_title == "POKEMON FIRE" && game_code == "BPRE" && game_version == 1) idle_loop_address = 0x80008be;
    // Pokemon - LeafGreen Version (USA)
    if (game_title == "POKEMON LEAF" && game_code == "BPGE" && game_version == 0) idle_loop_address = 0x80008aa;
    // Pokemon - LeafGreen Version (USA, Europe) (Rev 1)
    if (game_title == "POKEMON LEAF" && game_code == "BPGE" && game_version == 1) idle_loop_address = 0x80008be;
    // Super Monkey Ball Jr. (USA)
    if (game_title == "MONKEYBALLJR" && game_code == "ALUE" && game_version == 0) has_flash = has_sram = false;
}

static void gba_reset(bool keep_backup) {
    std::memset(cpu_ewram, 0, sizeof(cpu_ewram));
    std::memset(cpu_iwram, 0, sizeof(cpu_iwram));
    std::memset(&ioreg, 0, sizeof(ioreg));
    std::memset(palette_ram, 0, sizeof(palette_ram));
    std::memset(video_ram, 0, sizeof(video_ram));
    std::memset(object_ram, 0, sizeof(object_ram));
    if (!keep_backup) backup_erase();
    backup_init();
    gpio_init();

    std::memset(r, 0, sizeof(uint32_t) * 16);
    arm_init_registers(skip_bios);
    branch_taken = true;

    ppu_cycles = 0;
    halted = false;
    active_dma = -1;

    ioreg.dispcnt.w = 0x80;
    ioreg.bg_affine[0].pa.w = 0x100;
    ioreg.bg_affine[0].pd.w = 0x100;
    ioreg.bg_affine[1].pa.w = 0x100;
    ioreg.bg_affine[1].pd.w = 0x100;
    last_bios_access = 0;

    if (skip_bios) {
        ioreg.rcnt.w = 0x8000;
        ioreg.postflg = 1;
        last_bios_access = 0xe4;
    }
}

static void read_save_file() {
    SDL_RWops *rw = SDL_RWFromFile(save_path.c_str(), "rb");
    if (rw == nullptr) return;

    if (has_eeprom) {
        SDL_RWread(rw, backup_eeprom, sizeof(backup_eeprom), 1);
    } else if (has_flash) {
        SDL_RWread(rw, backup_flash, sizeof(backup_flash), 1);
    } else if (has_sram) {
        SDL_RWread(rw, backup_sram, sizeof(backup_sram), 1);
    }

    SDL_RWclose(rw);
}

static void write_save_file() {
    if (!has_eeprom && !has_flash && !has_sram) return;

    SDL_RWops *rw = SDL_RWFromFile(save_path.c_str(), "wb");
    if (rw == nullptr) return;

    if (has_eeprom) {
        SDL_RWwrite(rw, backup_eeprom, sizeof(backup_eeprom), 1);
    } else if (has_flash) {
        SDL_RWwrite(rw, backup_flash, sizeof(backup_flash), 1);
    } else if (has_sram) {
        SDL_RWwrite(rw, backup_sram, sizeof(backup_sram), 1);
    }

    SDL_RWclose(rw);
}

static void read_bios_file() {
    SDL_RWops *rw = SDL_RWFromFile("gba_bios.bin", "rb");
    if (rw == nullptr) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Missing BIOS file", "Failed to open BIOS file 'gba_bios.bin'.", nullptr);
        std::exit(EXIT_FAILURE);
    }

    SDL_RWread(rw, system_rom, sizeof(system_rom), 1);

    SDL_RWclose(rw);
}

static void read_rom_file(const std::string &rom_path) {
    std::memset(game_rom, 0, sizeof(game_rom));

    SDL_RWops *rw = SDL_RWFromFile(rom_path.c_str(), "rb");
    if (rw == nullptr) return;

    SDL_RWseek(rw, 0, RW_SEEK_END);
    game_rom_size = SDL_RWtell(rw);
    assert(game_rom_size != 0);
    game_rom_mask = std::bit_ceil(game_rom_size) - 1;
    SDL_RWseek(rw, 0, RW_SEEK_SET);
    if (game_rom_size <= sizeof(game_rom)) {
        SDL_RWread(rw, game_rom, game_rom_size, 1);
    }

    SDL_RWclose(rw);
}

static void gba_load(const std::string &rom_path) {
    const std::string rom_ext{".gba"};
    if (!rom_path.ends_with(rom_ext)) return;

    if (!save_path.empty()) {
        write_save_file();
    }
    save_path = rom_path;
    std::string::size_type n = save_path.rfind(rom_ext);
    save_path.replace(n, rom_ext.length(), ".sav");

    gba_reset(false);
    read_rom_file(rom_path);
    gba_detect_cartridge_features();
    read_save_file();
}

static void gba_ppu_update() {
    if (ppu_cycles % 1232 == 0) {
        ioreg.dispstat.w &= ~DSTAT_IN_HBL;
        if (ioreg.vcount.w < SCREEN_HEIGHT) {
            video_draw_scanline();
            video_affine_update();
        }
        ioreg.vcount.w = (ioreg.vcount.w + 1) % 228;
        if (ioreg.vcount.w == 227) {
            ioreg.dispstat.w &= ~DSTAT_IN_VBL;
        } else if (ioreg.vcount.w == 161) {  // FIXME
            if (ioreg.dispstat.w & DSTAT_VBL_IRQ) {
                ioreg.irq.w |= INT_VBLANK;
            }
        } else if (ioreg.vcount.w == 160) {
            if (!(ioreg.dispstat.w & DSTAT_IN_VBL)) {
                ioreg.dispstat.w |= DSTAT_IN_VBL;
                gba_dma_update(DMA_AT_VBLANK);
            }
        } else if (ioreg.vcount.w == 0) {
            video_affine_reset();
        }
        if (ioreg.vcount.w == ioreg.dispstat.b.b1) {
            if (!(ioreg.dispstat.w & DSTAT_IN_VCT)) {
                ioreg.dispstat.w |= DSTAT_IN_VCT;
                if (ioreg.dispstat.w & DSTAT_VCT_IRQ) {
                    ioreg.irq.w |= INT_VCOUNT;
                }
            }
        } else {
            ioreg.dispstat.w &= ~DSTAT_IN_VCT;
        }
    }
    ppu_cycles = (ppu_cycles + 1) % 280896;
    if (ppu_cycles % 1232 == 1006) {
        if (!(ioreg.dispstat.w & DSTAT_IN_HBL)) {
            ioreg.dispstat.w |= DSTAT_IN_HBL;
            if (ioreg.dispstat.w & DSTAT_HBL_IRQ) {
                ioreg.irq.w |= INT_HBLANK;
            }
            if (ppu_cycles < 197120) {
                gba_dma_update(DMA_AT_HBLANK);
            }
        }
    }
}

static void gba_timer_update(uint32_t cycles) {
    bool overflow = false;

    for (int i = 0; i < 4; i++) {
        uint16_t *counter = &ioreg.timer[i].counter.w;
        uint16_t *reload = &ioreg.timer[i].reload.w;
        uint16_t *control = &ioreg.timer[i].control.w;
        uint32_t *elapsed = &ioreg.timer[i].elapsed;

        if (!(*control & TM_ENABLE)) {
            overflow = false;
            continue;
        }

        uint32_t increment = 0;
        if (*control & TM_CASCADE) {
            increment = (overflow ? 1 : 0);
        } else {
            *elapsed += cycles;
            uint32_t freq;
            switch (*control & TM_FREQ_MASK) {
                case TM_FREQ_1: freq = 1; break;
                case TM_FREQ_64: freq = 64; break;
                case TM_FREQ_256: freq = 256; break;
                case TM_FREQ_1024: freq = 1024; break;
                default: std::abort();
            }
            if (*elapsed >= freq) {
                increment = *elapsed / freq;
                *elapsed = *elapsed % freq;
            }
        }

        uint16_t last_counter = *counter;
        *counter += increment;
        overflow = *counter < last_counter;
        if (overflow) {
            *counter += *reload;
            bool fifo_a_tick = BIT(ioreg.soundcnt_h.w, 10) == i;
            bool fifo_b_tick = BIT(ioreg.soundcnt_h.w, 14) == i;
            if (fifo_a_tick) {
                ioreg.fifo_a_ticks = (ioreg.fifo_a_ticks + 1) % 16;
                if (ioreg.fifo_a_ticks == 0) ioreg.fifo_a_refill = true;
            }
            if (fifo_b_tick) {
                ioreg.fifo_b_ticks = (ioreg.fifo_b_ticks + 1) % 16;
                if (ioreg.fifo_b_ticks == 0) ioreg.fifo_b_refill = true;
            }
            if (ioreg.fifo_a_refill || ioreg.fifo_b_refill) {
                gba_dma_update(DMA_AT_REFRESH);
                ioreg.fifo_a_refill = false;
                ioreg.fifo_b_refill = false;
            }
            if (*control & TM_IRQ) {
                ioreg.irq.w |= 1 << (3 + i);
            }
        }
    }
}

static void gba_dma_transfer(int ch, uint32_t dst_ctrl, uint32_t src_ctrl, uint32_t *dst_addr, uint32_t *src_addr, uint32_t size, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        bool bad_src_addr = !(*src_addr >= 0x02000000 && *src_addr < 0x10000000);
        bad_src_addr |= (ch == 0 && *src_addr >= 0x08000000 && *src_addr < 0x0e000000);

        bool bad_dst_addr = !(*dst_addr >= 0x02000000 && *dst_addr < 0x10000000);
        bad_dst_addr |= (ch != 3 && *dst_addr >= 0x08000000);

        if (size == 4) {
            uint32_t value = ioreg.dma_value.dw;
            if (!bad_src_addr) value = memory_read_word(*src_addr & ~3);
            ioreg.dma_value.dw = value;
            if (!bad_dst_addr) memory_write_word(*dst_addr & ~3, value);
        } else {
            uint16_t value = ioreg.dma_value.w.w0;
            if (!bad_src_addr) value = memory_read_halfword(*src_addr & ~1);
            ioreg.dma_value.w.w0 = value;
            if (!bad_dst_addr) memory_write_halfword(*dst_addr & ~1, value);
        }

        if (dst_ctrl == DMA_INC || dst_ctrl == DMA_RELOAD) {
            *dst_addr += size;
        } else if (dst_ctrl == DMA_DEC) {
            *dst_addr -= size;
        }
        if (src_ctrl == DMA_INC) {
            *src_addr += size;
        } else if (src_ctrl == DMA_DEC) {
            *src_addr -= size;
        }
    }
}

void gba_dma_reset(int ch) {
    uint32_t sad = ioreg.dma[ch].sad.dw;
    uint32_t dad = ioreg.dma[ch].dad.dw;
    uint32_t cnt = ioreg.dma[ch].cnt.dw;

    ioreg.dma[ch].src_addr = sad;
    ioreg.dma[ch].dst_addr = dad;
    ioreg.dma[ch].count = (uint16_t) cnt;
    if (ioreg.dma[ch].count == 0) ioreg.dma[ch].count = (ch == 3 ? 0x10000 : 0x4000);
}

void gba_dma_update(uint32_t current_timing) {
    for (int ch = 0; ch < 4; ch++) {
        uint32_t dad = ioreg.dma[ch].dad.dw;
        uint32_t cnt = ioreg.dma[ch].cnt.dw;
        uint32_t start_timing = BITS(cnt, 28, 29);

        if (!(cnt & DMA_ENABLE)) continue;
        if (start_timing != current_timing) continue;

        uint32_t *dst_addr = &ioreg.dma[ch].dst_addr;
        uint32_t *src_addr = &ioreg.dma[ch].src_addr;
        uint16_t count = ioreg.dma[ch].count;

        uint32_t dst_ctrl = BITS(cnt, 21, 22);
        uint32_t src_ctrl = BITS(cnt, 23, 24);
        if (*src_addr >= 0x08000000 && *src_addr < 0x0e000000) src_ctrl = DMA_INC;
        bool word_size = (cnt & DMA_32);

        if (start_timing == DMA_AT_VBLANK) assert(ioreg.vcount.w == 160);
        if (start_timing == DMA_AT_HBLANK) assert(ppu_cycles < 197120 && ppu_cycles % 1232 == 1006);
        if (start_timing == DMA_AT_REFRESH) {
            if (ch == 0) {
                continue;
            } else if (ch == 1 || ch == 2) {
                if (!(*dst_addr == 0x40000a0 || *dst_addr == 0x40000a4)) continue;
                assert(cnt & DMA_REPEAT);
                if (*dst_addr == 0x40000a0 && !ioreg.fifo_a_refill) continue;
                if (*dst_addr == 0x40000a4 && !ioreg.fifo_b_refill) continue;
                dst_ctrl = DMA_FIXED;
                word_size = true;
                count = 4;
            } else if (ch == 3) {
                continue;  // FIXME
            }
        }

        assert(src_ctrl != DMA_RELOAD);
        assert(!(cnt & DMA_DRQ));

        // EEPROM size autodetect
        if (has_eeprom && game_rom_size <= 0x1000000 && (*dst_addr >= 0x0d000000 && *dst_addr < 0x0e000000)) {
            if (count == 9 || count == 73) {
                eeprom_width = 6;
            } else if (count == 17 || count == 81) {
                eeprom_width = 14;
            }
        }

        active_dma = ch;
        gba_dma_transfer(ch, dst_ctrl, src_ctrl, dst_addr, src_addr, word_size ? 4 : 2, count);
        active_dma = -1;

        if (cnt & DMA_IRQ) {
            ioreg.irq.w |= 1 << (8 + ch);
        }

        if (cnt & DMA_REPEAT) {
            if (dst_ctrl == DMA_RELOAD) ioreg.dma[ch].dst_addr = dad;
            ioreg.dma[ch].count = (uint16_t) cnt;
            if (ioreg.dma[ch].count == 0) ioreg.dma[ch].count = (ch == 3 ? 0x10000 : 0x4000);
        } else {
            ioreg.dma[ch].cnt.dw &= ~DMA_ENABLE;
        }
    }
}

static void gba_emulate() {
    while (true) {
        int cpu_cycles = 0;

        if (FLAG_T()) {
            if (branch_taken) thumb_fill_pipeline();
            if (!halted) cpu_cycles = thumb_step();
        } else {
            if (branch_taken) arm_fill_pipeline();
            if (!halted) cpu_cycles = arm_step();
        }

        // Idle loop optimization
        if (idle_loop_address != 0 && idle_loop_address == get_pc()) {
            bool vblank_raised = (idle_loop_last_irq & INT_VBLANK);
            bool vblank_handled = !(ioreg.irq.w & INT_VBLANK);
            if (!(vblank_raised && vblank_handled)) {
                halted = true;
            }
            idle_loop_last_irq = ioreg.irq.w;
        }

        if (ioreg.irq.w & ioreg.ie.w) {
            halted = false;
            if (!branch_taken && !(cpsr & PSR_I) && ioreg.ime.w) {
                arm_hardware_interrupt();
            }
        }

        gba_timer_update(1);  // FIXME
        gba_ppu_update();
        if (ppu_cycles == 0 || (single_step && cpu_cycles > 0)) break;
    }
}

// Main code
int main(int argc, char *argv[]) {
    arm_init_lookup();
    thumb_init_lookup();

    read_bios_file();
    gba_reset(false);

    if (argc == 2) {
        skip_bios = true;
        const std::string rom_path(argv[1]);
        gba_load(rom_path);
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
    SDL_GameController *game_controller = nullptr;
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
                gba_load(rom_path);
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

        // Input
        const Uint8 *key_state = SDL_GetKeyboardState(nullptr);
        done |= (bool) key_state[SDL_SCANCODE_ESCAPE];
        static bool keys[10];
        std::memset(keys, 0, sizeof(keys));
        keys[0] |= (bool) key_state[SDL_SCANCODE_X];          // Button A
        keys[1] |= (bool) key_state[SDL_SCANCODE_Z];          // Button B
        keys[2] |= (bool) key_state[SDL_SCANCODE_BACKSPACE];  // Select
        keys[3] |= (bool) key_state[SDL_SCANCODE_RETURN];     // Start
        keys[4] |= (bool) key_state[SDL_SCANCODE_RIGHT];      // Right
        keys[5] |= (bool) key_state[SDL_SCANCODE_LEFT];       // Left
        keys[6] |= (bool) key_state[SDL_SCANCODE_UP];         // Up
        keys[7] |= (bool) key_state[SDL_SCANCODE_DOWN];       // Down
        keys[8] |= (bool) key_state[SDL_SCANCODE_S];          // Button R
        keys[9] |= (bool) key_state[SDL_SCANCODE_A];          // Button L
        if (game_controller != nullptr) {
            keys[0] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_A);              // Button A
            keys[1] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_B);              // Button B
            keys[2] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_BACK);           // Select
            keys[3] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_START);          // Start
            keys[4] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);     // Right
            keys[5] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);      // Left
            keys[6] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_DPAD_UP);        // Up
            keys[7] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);      // Down
            keys[8] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);  // Button R
            keys[9] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);   // Button L
        }
        if (keys[4] && keys[5]) keys[4] = keys[5] = false;  // Disallow opposing directions
        if (keys[6] && keys[7]) keys[6] = keys[7] = false;
        ioreg.keyinput.w = 0x3ff;
        for (int i = 0; i < 10; i++) {
            if (keys[i]) {
                ioreg.keyinput.w &= ~(1 << i);
            }
        }

        static bool paused = false;
        if (!paused) {
            gba_emulate();
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
                        uint16_t op = memory_read_halfword(address);
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", fmt::format("{:04X}", op).c_str());
                        ImGui::TableNextColumn();
                        thumb_disasm(address, op, disasm_text);
                    } else {
                        uint32_t op = memory_read_word(address);
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
        mem_edit.ReadFn = [](const uint8_t *data, std::size_t off) { UNUSED(data); return memory_read_byte(off); };
        mem_edit.WriteFn = [](uint8_t *data, std::size_t off, uint8_t d) { UNUSED(data); memory_write_byte(off, d); };
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
            gba_reset(true);
        }
        if (ImGui::Button("Manual save")) {
            write_save_file();
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
        write_save_file();
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
