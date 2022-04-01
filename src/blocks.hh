#pragma once

#include "block.hh"
#include "blocks/battery.hh"
#include "blocks/clock.hh"
#include "blocks/cpu.hh"
#include "blocks/fps.hh"
#include "blocks/memory.hh"

#include <filesystem>
#include <memory>

// clang-format off
static std::unique_ptr<Block> blocks[] = {
    std::make_unique<BatteryBlock>("/sys/class/power_supply/BAT0"),
    std::make_unique<MemoryBlock>(),
    std::make_unique<CpuBlock>(),
    std::make_unique<ClockBlock>(),
    std::make_unique<FpsBlock>(),
    // TODO: NvidiaGpuBlock
    // TODO: DiskBlock
    // TODO: NetworkBlock
    // TODO: PipewireBlock
    // TODO: ClockBlock
    // TODO: InternetBlock
};
// clang-format on
