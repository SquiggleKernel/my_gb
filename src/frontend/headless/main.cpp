#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "core/gb.h"

namespace {

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

void usage() {
    std::fprintf(stderr,
                 "usage: gb_headless <rom> [--frames N] [--serial] [--print-state]\n");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }

    std::string rom_path = argv[1];
    long frames = 600;
    bool show_serial = false;
    bool print_state = false;

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) {
            frames = std::strtol(argv[++i], nullptr, 10);
        } else if (arg == "--serial") {
            show_serial = true;
        } else if (arg == "--print-state") {
            print_state = true;
        } else {
            usage();
            return 2;
        }
    }

    std::vector<gb::u8> rom;
    if (!read_file(rom_path, rom)) {
        std::fprintf(stderr, "cannot read %s\n", rom_path.c_str());
        return 1;
    }

    gb::Gb machine;
    std::string err;
    if (!machine.load_rom(std::move(rom), &err)) {
        std::fprintf(stderr, "%s: %s\n", rom_path.c_str(), err.c_str());
        return 1;
    }

    std::string serial;
    if (show_serial) {
        machine.set_serial_sink([&serial](gb::u8 byte) {
            serial.push_back(static_cast<char>(byte));
        });
    }

    for (long i = 0; i < frames; ++i) {
        machine.run_frame();
    }

    if (show_serial) {
        std::fwrite(serial.data(), 1, serial.size(), stdout);
        std::fputc('\n', stdout);
    }

    if (print_state) {
        const gb::Regs& r = machine.cpu().regs();
        std::printf("AF:%04X BC:%04X DE:%04X HL:%04X SP:%04X PC:%04X cycles:%llu\n", r.af(),
                    r.bc(), r.de(), r.hl(), r.sp, r.pc,
                    static_cast<unsigned long long>(machine.bus().cycles()));
    }

    return 0;
}
