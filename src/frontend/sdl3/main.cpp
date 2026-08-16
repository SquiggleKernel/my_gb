#include <SDL3/SDL.h>

#include <array>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "core/gb.h"

namespace {

constexpr int kScale = 4;

// The classic DMG green, darkest last.
constexpr std::array<gb::u32, 4> kPalette = {0xFF9BBC0F, 0xFF8BAC0F, 0xFF306230, 0xFF0F380F};

bool read_file(const std::string& path, std::vector<gb::u8>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        return false;
    }
    const std::streamsize size = f.tellg();
    if (size < 0) {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    f.seekg(0);
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), size));
}

gb::u8 button_for(SDL_Keycode key) {
    switch (key) {
        case SDLK_RIGHT: return gb::kBtnRight;
        case SDLK_LEFT: return gb::kBtnLeft;
        case SDLK_UP: return gb::kBtnUp;
        case SDLK_DOWN: return gb::kBtnDown;
        case SDLK_Z: return gb::kBtnA;
        case SDLK_X: return gb::kBtnB;
        case SDLK_RSHIFT: return gb::kBtnSelect;
        case SDLK_RETURN: return gb::kBtnStart;
        default: return 0;
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: gb_sdl3 <rom>\n");
        return 2;
    }

    std::vector<gb::u8> rom;
    if (!read_file(argv[1], rom)) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }

    gb::Gb machine;
    std::string err;
    if (!machine.load_rom(std::move(rom), &err)) {
        std::fprintf(stderr, "%s: %s\n", argv[1], err.c_str());
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("my_gb", gb::kScreenWidth * kScale,
                                          gb::kScreenHeight * kScale, SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, gb::kScreenWidth, gb::kScreenHeight,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_Texture* texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                          gb::kScreenWidth, gb::kScreenHeight);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    std::array<gb::u32, gb::kFramebufferPixels> pixels{};
    gb::u8 buttons = 0;
    bool running = true;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                buttons = static_cast<gb::u8>(buttons | button_for(ev.key.key));
            } else if (ev.type == SDL_EVENT_KEY_UP) {
                buttons = static_cast<gb::u8>(buttons & ~button_for(ev.key.key));
            }
        }

        machine.set_buttons(buttons);
        machine.run_frame();

        const auto& fb = machine.framebuffer();
        for (std::size_t i = 0; i < fb.size(); ++i) {
            pixels[i] = kPalette[fb[i] & 3];
        }
        SDL_UpdateTexture(texture, nullptr, pixels.data(), gb::kScreenWidth * 4);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
