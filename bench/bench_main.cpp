#include <benchmark/benchmark.h>

#include <fstream>
#include <string>
#include <vector>

#include "core/gb.h"

namespace {

// Falls back to a generated ROM so the benchmarks still run on a checkout
// where tools/fetch_test_roms.py has not been run.
std::vector<gb::u8> synthetic_rom() {
    std::vector<gb::u8> rom(32768, 0x00);
    rom[0x147] = 0x00;
    rom[0x148] = 0x00;
    rom[0x149] = 0x00;

    // Turn the LCD on with the background enabled, then spin through a mix of
    // arithmetic and memory traffic so the CPU, PPU and timer all stay busy.
    const gb::u8 program[] = {
        0x3E, 0x91,        // LD A,$91
        0xE0, 0x40,        // LDH ($40),A   LCDC
        0x21, 0x00, 0x80,  // LD HL,$8000
        0x0E, 0x00,        // LD C,$00
        0x79,              // LD A,C
        0x22,              // LD (HL+),A
        0x0C,              // INC C
        0x3C,              // INC A
        0x87,              // ADD A,A
        0xCB, 0x37,        // SWAP A
        0x7C,              // LD A,H
        0xFE, 0xA0,        // CP $A0
        0x20, 0xF4,        // JR NZ,-12
        0x18, 0xE8,        // JR -24
    };
    for (std::size_t i = 0; i < sizeof(program); ++i) {
        rom[0x100 + i] = program[i];
    }
    return rom;
}

std::vector<gb::u8> load_rom() {
    const std::string path = std::string(GB_ROM_DIR) + "/blargg/cpu_instrs/cpu_instrs.gb";
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        return synthetic_rom();
    }
    const std::streamsize size = f.tellg();
    if (size <= 0) {
        return synthetic_rom();
    }
    std::vector<gb::u8> rom(static_cast<std::size_t>(size));
    f.seekg(0);
    if (!f.read(reinterpret_cast<char*>(rom.data()), size)) {
        return synthetic_rom();
    }
    return rom;
}

void boot(gb::Gb& machine) {
    std::string err;
    machine.load_rom(load_rom(), &err);
    // Skip past the setup the ROM does before it settles into its main loop, so
    // the measured region is representative.
    for (int i = 0; i < 60; ++i) {
        machine.run_frame();
    }
}

void BM_Frame(benchmark::State& state) {
    gb::Gb machine;
    boot(machine);
    for (auto _ : state) {
        machine.run_frame();
    }
    state.SetItemsProcessed(state.iterations());
    state.counters["fps"] = benchmark::Counter(static_cast<double>(state.iterations()),
                                               benchmark::Counter::kIsRate);
}
BENCHMARK(BM_Frame)->Unit(benchmark::kMillisecond);

void BM_Dispatch(benchmark::State& state) {
    gb::Gb machine;
    boot(machine);
    for (auto _ : state) {
        machine.step_instruction();
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Dispatch);

void BM_SaveState(benchmark::State& state) {
    gb::Gb machine;
    boot(machine);
    for (auto _ : state) {
        benchmark::DoNotOptimize(machine.save_state());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SaveState)->Unit(benchmark::kMicrosecond);

}  // namespace
