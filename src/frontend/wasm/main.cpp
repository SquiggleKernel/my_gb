#include <emscripten.h>
#include <emscripten/html5.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/gb.h"

namespace {

// The classic DMG green, darkest last. ImageData is RGBA byte order, so on a
// little-endian target each word is written 0xAABBGGRR.
constexpr std::array<gb::u32, 4> kPalette = {0xFF0FBC9B, 0xFF0FAC8B, 0xFF306230, 0xFF0F380F};

gb::Gb machine;
std::array<gb::u32, gb::kFramebufferPixels> pixels{};
gb::u8 buttons = 0;
bool running = false;

gb::u8 button_for(const char* code) {
    const std::string key = code;
    if (key == "ArrowRight") return gb::kBtnRight;
    if (key == "ArrowLeft") return gb::kBtnLeft;
    if (key == "ArrowUp") return gb::kBtnUp;
    if (key == "ArrowDown") return gb::kBtnDown;
    if (key == "KeyZ") return gb::kBtnA;
    if (key == "KeyX") return gb::kBtnB;
    if (key == "ShiftRight") return gb::kBtnSelect;
    if (key == "Enter") return gb::kBtnStart;
    return 0;
}

EM_BOOL on_key(int type, const EmscriptenKeyboardEvent* ev, void* user) {
    (void)user;
    const gb::u8 mask = button_for(ev->code);
    if (mask == 0) {
        return EM_FALSE;
    }
    if (type == EMSCRIPTEN_EVENT_KEYDOWN) {
        buttons = static_cast<gb::u8>(buttons | mask);
    } else {
        buttons = static_cast<gb::u8>(buttons & ~mask);
    }
    return EM_TRUE;
}

void frame() {
    if (!running) {
        return;
    }

    machine.set_buttons(buttons);
    machine.run_frame();

    const auto& fb = machine.framebuffer();
    for (std::size_t i = 0; i < fb.size(); ++i) {
        pixels[i] = kPalette[fb[i] & 3];
    }

    MAIN_THREAD_EM_ASM({ gbDrawFrame($0, $1, $2); }, pixels.data(), gb::kScreenWidth,
                       gb::kScreenHeight);
}

}  // namespace

extern "C" {

// Called from JS once the user has picked a file; the buffer is a copy owned by
// the JS side and freed there right after this returns.
EMSCRIPTEN_KEEPALIVE int gb_load_rom(const std::uint8_t* data, int size) {
    if (data == nullptr || size <= 0) {
        return 0;
    }
    std::vector<gb::u8> rom(data, data + size);
    std::string err;
    if (!machine.load_rom(std::move(rom), &err)) {
        MAIN_THREAD_EM_ASM({ gbReportError(UTF8ToString($0)); }, err.c_str());
        return 0;
    }
    buttons = 0;
    running = true;
    return 1;
}

EMSCRIPTEN_KEEPALIVE void gb_reset() {
    machine.reset();
    buttons = 0;
}

}  // extern "C"

int main() {
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, on_key);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, on_key);
    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}
