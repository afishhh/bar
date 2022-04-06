#pragma once

#include "block.hh"
#include "blocks/battery.hh"
#include "blocks/clock.hh"
#include "blocks/cpu.hh"
#include "blocks/disk.hh"
#include "blocks/dwm.hh"
#include "blocks/fps.hh"
#include "blocks/memory.hh"
#include "blocks/network.hh"
#include "blocks/script.hh"

#include <csignal>
#include <cstddef>
#include <filesystem>
#include <memory>

using namespace std::literals;

namespace config {

// NOTE: While the height might seem configurable you will also need to change
//       widths of quite a few different independent boxes
static size_t height = 24;
static bool override_redirect = false;

static std::initializer_list<const char *> fonts = {
    "monospace:size=10", "Font Awesome 5 Free Solid:style=Solid:size=9"};

// clang-format off
// FIXME: Allow right aligned blocks.
static std::unique_ptr<Block> blocks[] = {
    std::make_unique<DwmBlock>("/tmp/dwm.socket"),
    std::make_unique<ClockBlock>(),
    std::make_unique<BatteryBlock>("/sys/class/power_supply/BAT0",
        BatteryBlock::Config {
            .prefix = "BAT ",
            .show_percentage = true,
            .show_time_left_charging = true,
            .show_time_left_discharging = true,
            // How many "blocks" of time to show.
            // eg. if 2 then "2h 30m" will be shown.
            // if 3 then "2h 30m 10s" will be shown.
            .time_precision = 2,
            .bar_width = 75,
            .show_wattage = true,
            .show_degradation = true,
        }
    ),
    std::make_unique<MemoryBlock>(),
    std::make_unique<CpuBlock>(
        CpuBlock::Config {
            .thermal_zone_type = "SEN1"
        }
    ),
    std::make_unique<DiskBlock>("/",
        DiskBlock::Config {
            .show_fs_type = false,
            .show_usage_text = true,
            .show_usage_bar = true,
            .bar_width = 45,
            // If the color is not set then it's chosen based on the usage percentage.
            // .bar_fill_color = 0x00FF00,
        }
    ),
    std::make_unique<NetworkBlock>(),
    std::make_unique<FpsBlock>(),
    // TODO: NvidiaGpuBlock
    // TODO: PipewireBlock
    // TODO: InternetBlock
    // TODO: DwmBlock
};
// clang-format on

} // namespace config
