#pragma once

#include "block.hh"
#include "blocks/battery.hh"
#include "blocks/clock.hh"
#include "blocks/cpu.hh"
#include "blocks/fps.hh"
#include "blocks/memory.hh"
#include "blocks/network.hh"
#include "blocks/script.hh"

#include <filesystem>
#include <memory>

using namespace std::literals;

static std::initializer_list<const char *> font_names = {
    "monospace:size=10", "Font Awesome 5 Free Solid:style=Solid:size=9"};

// clang-format off
static std::unique_ptr<Block> blocks[] = {
    std::make_unique<BatteryBlock>("/sys/class/power_supply/BAT0"),
    std::make_unique<MemoryBlock>(),
    std::make_unique<CpuBlock>(),
    std::make_unique<ClockBlock>(),
    std::make_unique<NetworkBlock>(),

    std::make_unique<ScriptBlock>("sb-mic-volume", 500ms),

    std::make_unique<FpsBlock>(),
    // TODO: NvidiaGpuBlock
    // TODO: DiskBlock
    // TODO: PipewireBlock
    // TODO: InternetBlock
    // TODO: DwmBlock
};
// clang-format on
