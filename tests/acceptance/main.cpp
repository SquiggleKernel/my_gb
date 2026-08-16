#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "core/gb.h"
#include "png.h"
#include "sha256.h"

namespace fs = std::filesystem;

namespace {

enum class Kind { Blargg, Mooneye, Framebuffer };

struct Options {
    std::string roms = "tests/roms";
    std::string golden = "tests/golden";
    std::string filter;
    std::string dump_png;
    long timeout_frames = 6000;
    bool update_golden = false;
    bool verbose = false;
};

struct Outcome {
    int status = 0;  // 0 pass, 1 fail, 2 skip
    std::string reason;
    std::string hash;
};

bool read_file(const fs::path& path, std::vector<gb::u8>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        return false;
    }
    const std::streamsize size = f.tellg();
    if (size <= 0) {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    f.seekg(0);
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), size));
}

Kind classify(const std::string& rel) {
    if (rel.find("blargg") != std::string::npos) {
        return Kind::Blargg;
    }
    if (rel.find("mooneye") != std::string::npos) {
        return Kind::Mooneye;
    }
    return Kind::Framebuffer;
}

// Several Blargg suites (halt_bug, oam_bug, mem_timing-2) are built without
// serial output and report only on the LCD, so the result word has to be read
// off the framebuffer. These are the 6x1 tile bitmaps the shared font renders
// for "Passed" and "Failed", one byte per 8 pixels, row major.
constexpr std::array<gb::u8, 48> kPassedGlyphs = {
    0x7C, 0x00, 0x00, 0x00, 0x00, 0x06, 0x66, 0x00, 0x00, 0x00, 0x00, 0x06,
    0x66, 0x3C, 0x3E, 0x3E, 0x3C, 0x3E, 0x7C, 0x06, 0x60, 0x60, 0x66, 0x66,
    0x60, 0x3E, 0x3C, 0x3C, 0x7E, 0x66, 0x60, 0x66, 0x06, 0x06, 0x60, 0x66,
    0x60, 0x3E, 0x7C, 0x7C, 0x3C, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

constexpr std::array<gb::u8, 48> kFailedGlyphs = {
    0x7E, 0x00, 0x18, 0x38, 0x00, 0x06, 0x60, 0x00, 0x00, 0x18, 0x00, 0x06,
    0x60, 0x3C, 0x38, 0x18, 0x3C, 0x3E, 0x7C, 0x06, 0x18, 0x18, 0x66, 0x66,
    0x60, 0x3E, 0x18, 0x18, 0x7E, 0x66, 0x60, 0x66, 0x18, 0x18, 0x60, 0x66,
    0x60, 0x3E, 0x3C, 0x3C, 0x3C, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

constexpr int kWordTiles = 6;

bool word_at(const std::array<gb::u8, gb::kFramebufferPixels>& fb, int tx, int ty,
             const std::array<gb::u8, 48>& glyphs) {
    for (int y = 0; y < 8; ++y) {
        for (int t = 0; t < kWordTiles; ++t) {
            gb::u8 row = 0;
            for (int x = 0; x < 8; ++x) {
                const int px = (tx + t) * 8 + x;
                const int py = ty * 8 + y;
                const std::size_t idx =
                    static_cast<std::size_t>(py) * gb::kScreenWidth + static_cast<std::size_t>(px);
                if (fb[idx] >= 2) {
                    row = static_cast<gb::u8>(row | (1 << (7 - x)));
                }
            }
            if (row != glyphs[static_cast<std::size_t>(y * kWordTiles + t)]) {
                return false;
            }
        }
    }
    return true;
}

bool screen_has_word(const std::array<gb::u8, gb::kFramebufferPixels>& fb,
                     const std::array<gb::u8, 48>& glyphs) {
    for (int ty = 0; ty < gb::kScreenHeight / 8; ++ty) {
        for (int tx = 0; tx + kWordTiles <= gb::kScreenWidth / 8; ++tx) {
            if (word_at(fb, tx, ty, glyphs)) {
                return true;
            }
        }
    }
    return false;
}

std::string last_line(const std::string& text) {
    std::string trimmed = text;
    while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r')) {
        trimmed.pop_back();
    }
    const std::size_t nl = trimmed.find_last_of('\n');
    std::string line = nl == std::string::npos ? trimmed : trimmed.substr(nl + 1);
    for (char& c : line) {
        if (c < 0x20 || c > 0x7E) {
            c = ' ';
        }
    }
    return line;
}

// Mooneye and dmg-acid2 both signal completion by executing LD B,B.
bool run_to_breakpoint(gb::Gb& machine, gb::u64 cycle_cap) {
    const gb::u64 stop_at = machine.bus().cycles() + cycle_cap;
    while (machine.bus().cycles() < stop_at) {
        if (machine.bus().peek(machine.cpu().regs().pc) == 0x40) {
            return true;
        }
        machine.step_instruction();
    }
    return false;
}

Outcome run_blargg(gb::Gb& machine, const Options& opt) {
    std::string serial;
    machine.set_serial_sink([&serial](gb::u8 byte) { serial.push_back(static_cast<char>(byte)); });

    for (long i = 0; i < opt.timeout_frames; ++i) {
        machine.run_frame();
        if (serial.find("Passed") != std::string::npos) {
            return {0, "", ""};
        }
        if (serial.find("Failed") != std::string::npos) {
            return {1, last_line(serial), ""};
        }
        // Checked periodically rather than every frame; the scan is not free
        // and the result stays on screen once printed.
        if (i % 20 == 19) {
            const auto& fb = machine.framebuffer();
            if (screen_has_word(fb, kPassedGlyphs)) {
                return {0, "", ""};
            }
            if (screen_has_word(fb, kFailedGlyphs)) {
                return {1, "screen reports Failed", ""};
            }
        }
    }
    return {1, serial.empty() ? "timeout, no serial output" : "timeout: " + last_line(serial), ""};
}

Outcome run_mooneye(gb::Gb& machine, const Options& opt) {
    const gb::u64 cap = static_cast<gb::u64>(opt.timeout_frames) * gb::kTCyclesPerFrame;
    if (!run_to_breakpoint(machine, cap)) {
        return {1, "timeout waiting for LD B,B", ""};
    }
    const gb::Regs& r = machine.cpu().regs();
    if (r.b == 3 && r.c == 5 && r.d == 8 && r.e == 13 && r.h == 21 && r.l == 34) {
        return {0, "", ""};
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "B:%u C:%u D:%u E:%u H:%u L:%u", r.b, r.c, r.d, r.e, r.h, r.l);
    return {1, buf, ""};
}

Outcome run_framebuffer(gb::Gb& machine, const Options& opt) {
    const gb::u64 cap = static_cast<gb::u64>(opt.timeout_frames) * gb::kTCyclesPerFrame;
    run_to_breakpoint(machine, cap);
    // The run stops at an arbitrary instruction, which leaves the framebuffer
    // holding the top of the current frame and the bottom of the previous one.
    // Finish the frame first so the hash does not depend on where we stopped.
    machine.run_frame();
    const auto& fb = machine.framebuffer();
    return {0, "", gbtest::sha256_hex(fb.data(), fb.size())};
}

std::map<std::string, std::string> load_golden(const fs::path& path) {
    std::map<std::string, std::string> out;
    std::ifstream f(path);
    if (!f) {
        return out;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::size_t sp = line.find_last_of(" \t");
        if (sp == std::string::npos) {
            continue;
        }
        out[line.substr(0, sp)] = line.substr(sp + 1);
    }
    return out;
}

void dump_png(const Options& opt, const std::string& rel, const gb::Gb& machine) {
    fs::path out = fs::path(opt.dump_png) / rel;
    out.replace_extension(".png");
    std::error_code ec;
    fs::create_directories(out.parent_path(), ec);

    const auto& fb = machine.framebuffer();
    std::vector<gb::u8> grey(fb.size());
    static constexpr gb::u8 kShade[4] = {255, 170, 85, 0};
    for (std::size_t i = 0; i < fb.size(); ++i) {
        grey[i] = kShade[fb[i] & 3];
    }
    gbtest::write_grey_png(out.string(), grey.data(), gb::kScreenWidth, gb::kScreenHeight);
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--roms" && i + 1 < argc) {
            opt.roms = argv[++i];
        } else if (arg == "--golden" && i + 1 < argc) {
            opt.golden = argv[++i];
        } else if (arg == "--filter" && i + 1 < argc) {
            opt.filter = argv[++i];
        } else if (arg == "--dump-png" && i + 1 < argc) {
            opt.dump_png = argv[++i];
        } else if (arg == "--timeout-frames" && i + 1 < argc) {
            opt.timeout_frames = std::strtol(argv[++i], nullptr, 10);
        } else if (arg == "--update-golden") {
            opt.update_golden = true;
        } else if (arg == "--verbose") {
            opt.verbose = true;
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", arg.c_str());
            return 2;
        }
    }

    std::error_code ec;
    if (!fs::is_directory(opt.roms, ec)) {
        std::printf("no test ROMs found in %s; run tools/fetch_test_roms.py\n", opt.roms.c_str());
        return 0;
    }

    std::vector<fs::path> roms;
    for (const auto& entry : fs::recursive_directory_iterator(opt.roms, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".gb") {
            roms.push_back(entry.path());
        }
    }
    std::sort(roms.begin(), roms.end());

    if (roms.empty()) {
        std::printf("no test ROMs found in %s; run tools/fetch_test_roms.py\n", opt.roms.c_str());
        return 0;
    }

    const auto golden = load_golden(fs::path(opt.golden) / "framebuffers.txt");
    std::map<std::string, std::string> observed;

    int passed = 0;
    int failed = 0;
    int skipped = 0;

    for (const auto& path : roms) {
        const std::string rel = fs::relative(path, opt.roms).generic_string();
        if (!opt.filter.empty() && rel.find(opt.filter) == std::string::npos) {
            continue;
        }

        std::vector<gb::u8> rom;
        if (!read_file(path, rom)) {
            std::printf("FAIL %s - cannot read file\n", rel.c_str());
            ++failed;
            continue;
        }

        gb::Gb machine;
        std::string err;
        if (!machine.load_rom(std::move(rom), &err)) {
            std::printf("FAIL %s - %s\n", rel.c_str(), err.c_str());
            ++failed;
            continue;
        }

        const Kind kind = classify(rel);
        Outcome result;
        switch (kind) {
            case Kind::Blargg: result = run_blargg(machine, opt); break;
            case Kind::Mooneye: result = run_mooneye(machine, opt); break;
            case Kind::Framebuffer: result = run_framebuffer(machine, opt); break;
        }

        if (!opt.dump_png.empty()) {
            dump_png(opt, rel, machine);
        }

        if (kind == Kind::Framebuffer) {
            observed[rel] = result.hash;
            const auto it = golden.find(rel);
            if (it == golden.end()) {
                std::printf("SKIP %s - no golden hash (%s)\n", rel.c_str(), result.hash.c_str());
                ++skipped;
                continue;
            }
            if (it->second != result.hash) {
                std::printf("FAIL %s - framebuffer %s, expected %s\n", rel.c_str(),
                            result.hash.c_str(), it->second.c_str());
                ++failed;
                continue;
            }
        }

        if (result.status == 0) {
            if (opt.verbose) {
                std::printf("PASS %s\n", rel.c_str());
            }
            ++passed;
        } else {
            std::printf("FAIL %s - %s\n", rel.c_str(), result.reason.c_str());
            ++failed;
        }
    }

    if (opt.update_golden && !observed.empty()) {
        const fs::path out = fs::path(opt.golden) / "framebuffers.txt";
        fs::create_directories(opt.golden, ec);
        std::ofstream f(out);
        f << "# <rom path relative to tests/roms> <sha256 of the 160x144 colour index buffer>\n";
        for (const auto& [rel, hash] : observed) {
            f << rel << ' ' << hash << '\n';
        }
        std::printf("wrote %zu golden hashes to %s\n", observed.size(), out.string().c_str());
    }

    std::printf("%d passed, %d failed, %d skipped\n", passed, failed, skipped);
    return failed > 0 ? 1 : 0;
}
