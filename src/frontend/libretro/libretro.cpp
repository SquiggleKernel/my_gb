#include <libretro.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/gb.h"

namespace {

// The classic DMG green, darkest last.
constexpr std::array<gb::u32, 4> kPalette = {0xFF9BBC0F, 0xFF8BAC0F, 0xFF306230, 0xFF0F380F};

constexpr unsigned kSampleRate = 48000;
// One frame of audio at 60 Hz is 800 frames; leave room for the APU running long.
constexpr std::size_t kAudioFrames = 2048;

struct ButtonMap {
    unsigned id;
    gb::u8 mask;
};

constexpr std::array<ButtonMap, 8> kButtons = {{
    {RETRO_DEVICE_ID_JOYPAD_RIGHT, gb::kBtnRight},
    {RETRO_DEVICE_ID_JOYPAD_LEFT, gb::kBtnLeft},
    {RETRO_DEVICE_ID_JOYPAD_UP, gb::kBtnUp},
    {RETRO_DEVICE_ID_JOYPAD_DOWN, gb::kBtnDown},
    {RETRO_DEVICE_ID_JOYPAD_A, gb::kBtnA},
    {RETRO_DEVICE_ID_JOYPAD_B, gb::kBtnB},
    {RETRO_DEVICE_ID_JOYPAD_SELECT, gb::kBtnSelect},
    {RETRO_DEVICE_ID_JOYPAD_START, gb::kBtnStart},
}};

retro_environment_t env_cb = nullptr;
retro_video_refresh_t video_cb = nullptr;
retro_audio_sample_t audio_cb = nullptr;
retro_audio_sample_batch_t audio_batch_cb = nullptr;
retro_input_poll_t input_poll_cb = nullptr;
retro_input_state_t input_state_cb = nullptr;

std::unique_ptr<gb::Gb> machine;
bool game_loaded = false;
std::size_t serialize_bound = 0;

std::array<gb::u32, gb::kFramebufferPixels> video;
std::vector<float> audio_float;
std::vector<std::int16_t> audio_pcm;
std::vector<gb::u8> state_scratch;

void log_fmt(retro_log_level level, const char* msg) {
    if (env_cb == nullptr) {
        return;
    }
    retro_log_callback log{};
    if (env_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log) && log.log != nullptr) {
        log.log(level, "%s", msg);
    }
}

std::int16_t to_pcm(float sample) {
    const float clamped = sample < -1.0F ? -1.0F : (sample > 1.0F ? 1.0F : sample);
    return static_cast<std::int16_t>(clamped * 32767.0F);
}

void emit_audio() {
    const std::size_t frames = machine->bus().apu().take_samples(audio_float.data(), kAudioFrames);
    for (std::size_t i = 0; i < frames * 2; ++i) {
        audio_pcm[i] = to_pcm(audio_float[i]);
    }
    if (frames != 0 && audio_batch_cb != nullptr) {
        audio_batch_cb(audio_pcm.data(), frames);
    }
}

gb::u8 poll_buttons() {
    if (input_state_cb == nullptr) {
        return 0;
    }
    gb::u8 pressed = 0;
    for (const ButtonMap& b : kButtons) {
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, b.id) != 0) {
            pressed = static_cast<gb::u8>(pressed | b.mask);
        }
    }
    return pressed;
}

// States are variable-length, so quantise the observed size upward and keep the
// largest bound we have ever reported. A frontend may cache the first answer.
void update_serialize_bound() {
    constexpr std::size_t kGranularity = 4096;
    const std::size_t size = machine->save_state().size();
    const std::size_t rounded = ((size + kGranularity) / kGranularity) * kGranularity;
    if (rounded > serialize_bound) {
        serialize_bound = rounded;
    }
}

}  // namespace

extern "C" {

void retro_set_environment(retro_environment_t cb) {
    env_cb = cb;
    bool no_game = false;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_init() {
    machine = std::make_unique<gb::Gb>();
    machine->bus().apu().set_sample_rate(kSampleRate);
    audio_float.assign(kAudioFrames * 2, 0.0F);
    audio_pcm.assign(kAudioFrames * 2, 0);
    game_loaded = false;
    serialize_bound = 0;
}

void retro_deinit() {
    machine.reset();
    audio_float.clear();
    audio_pcm.clear();
    state_scratch.clear();
    game_loaded = false;
    serialize_bound = 0;
}

unsigned retro_api_version() { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->library_name = "my_gb";
    info->library_version = "0.1";
    info->valid_extensions = "gb";
    info->need_fullpath = false;
    info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->geometry.base_width = static_cast<unsigned>(gb::kScreenWidth);
    info->geometry.base_height = static_cast<unsigned>(gb::kScreenHeight);
    info->geometry.max_width = static_cast<unsigned>(gb::kScreenWidth);
    info->geometry.max_height = static_cast<unsigned>(gb::kScreenHeight);
    info->geometry.aspect_ratio =
        static_cast<float>(gb::kScreenWidth) / static_cast<float>(gb::kScreenHeight);
    info->timing.fps = static_cast<double>(gb::kTCyclesPerSecond) /
                       static_cast<double>(gb::kTCyclesPerFrame);
    info->timing.sample_rate = static_cast<double>(kSampleRate);
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
    (void)port;
    (void)device;
}

void retro_reset() {
    machine->reset();
    machine->bus().apu().clear_samples();
}

void retro_run() {
    if (input_poll_cb != nullptr) {
        input_poll_cb();
    }
    machine->set_buttons(poll_buttons());
    machine->run_frame();

    const auto& fb = machine->framebuffer();
    for (std::size_t i = 0; i < fb.size(); ++i) {
        video[i] = kPalette[fb[i] & 3];
    }
    if (video_cb != nullptr) {
        video_cb(video.data(), static_cast<unsigned>(gb::kScreenWidth),
                 static_cast<unsigned>(gb::kScreenHeight),
                 static_cast<std::size_t>(gb::kScreenWidth) * sizeof(gb::u32));
    }

    emit_audio();
}

bool retro_load_game(const struct retro_game_info* game) {
    if (game == nullptr || game->data == nullptr || game->size == 0) {
        return false;
    }

    retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (env_cb == nullptr || !env_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        log_fmt(RETRO_LOG_ERROR, "XRGB8888 is not supported by this frontend");
        return false;
    }

    const auto* bytes = static_cast<const gb::u8*>(game->data);
    std::vector<gb::u8> rom(bytes, bytes + game->size);

    std::string err;
    if (!machine->load_rom(std::move(rom), &err)) {
        log_fmt(RETRO_LOG_ERROR, err.c_str());
        return false;
    }

    machine->bus().apu().set_sample_rate(kSampleRate);
    machine->bus().apu().clear_samples();
    game_loaded = true;
    serialize_bound = 0;
    update_serialize_bound();
    return true;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info* info, size_t num) {
    (void)type;
    (void)info;
    (void)num;
    return false;
}

void retro_unload_game() {
    game_loaded = false;
    serialize_bound = 0;
}

unsigned retro_get_region() { return RETRO_REGION_NTSC; }

void* retro_get_memory_data(unsigned id) {
    if (id != RETRO_MEMORY_SAVE_RAM || !game_loaded) {
        return nullptr;
    }
    const gb::Cartridge* cart = machine->bus().cartridge();
    if (cart == nullptr || !cart->has_battery() || cart->ram().empty()) {
        return nullptr;
    }
    // The core owns the buffer and writes through it during emulation; the span
    // is const only because ram() is an inspection accessor.
    return const_cast<gb::u8*>(cart->ram().data());
}

size_t retro_get_memory_size(unsigned id) {
    if (id != RETRO_MEMORY_SAVE_RAM || !game_loaded) {
        return 0;
    }
    const gb::Cartridge* cart = machine->bus().cartridge();
    if (cart == nullptr || !cart->has_battery()) {
        return 0;
    }
    return cart->ram().size();
}

size_t retro_serialize_size() {
    if (!game_loaded) {
        return 0;
    }
    return serialize_bound;
}

bool retro_serialize(void* data, size_t size) {
    if (!game_loaded || data == nullptr) {
        return false;
    }
    machine->write_state(state_scratch);
    if (state_scratch.size() > size) {
        return false;
    }
    std::memcpy(data, state_scratch.data(), state_scratch.size());
    std::memset(static_cast<gb::u8*>(data) + state_scratch.size(), 0,
                size - state_scratch.size());
    return true;
}

bool retro_unserialize(const void* data, size_t size) {
    if (!game_loaded || data == nullptr) {
        return false;
    }
    const auto* bytes = static_cast<const gb::u8*>(data);
    // Trailing padding is ignored: the archive stops once the payload is read.
    return machine->load_state(std::span<const gb::u8>(bytes, size));
}

void retro_cheat_reset() {}

void retro_cheat_set(unsigned index, bool enabled, const char* code) {
    (void)index;
    (void)enabled;
    (void)code;
}

}  // extern "C"
