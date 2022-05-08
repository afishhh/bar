#pragma once

#include "block.hh"
#include "blocks/battery.hh"
#include "blocks/clock.hh"
#include "blocks/cpu.hh"
#include "blocks/disk.hh"
#ifdef HAVE_DWMIPCPP
#include "blocks/dwm.hh"
#endif
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
const static size_t height = 24;

// Configuration options specific to the X11 backend
namespace x11 {

// Whether to set the override-redirect flag on the window
const static bool override_redirect = false;
// Window class to set for the window
// NOTE: This cannot be const due to bad const-correctness in X11
static std::string window_class = "bar";

} // namespace x11

const static std::initializer_list<const char *> fonts = {
    "monospace:size=10", "Font Awesome 5 Free Solid:style=Solid:size=9"};

// clang-format off
// FIXME: Allow right aligned blocks.
const static std::unique_ptr<Block> left_blocks[] = {
    std::make_unique<DwmBlock>(
        DwmBlock::Config {
            .socket_path = "/tmp/dwm.socket",
            .show_empty_tags = false,
            .inactive_tag_color = 0xFFFFFF,
            .selected_tag_color = 0x00FF00,
            .urgent_tag_color = 0xFF0000,
            .empty_tag_color = 0xFFFFFF,

            .floating_title_prefix = " ",
            .title_color = 0xFFFFFF,
            .floating_title_color = 0xFFFFFF,
        }
    ),
};
const static std::unique_ptr<Block> right_blocks[] = {
    std::make_unique<ClockBlock>(),
    std::make_unique<MemoryBlock>(
        MemoryBlock::Config {
            .prefix = "MEM ",
        }
    ),
    std::make_unique<CpuBlock>(
        CpuBlock::Config {
            .prefix = "CPU ",
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
    std::make_unique<ScriptBlock>("sb-mic-volume", 1s, SIGRTMIN + 20),
    std::make_unique<ScriptBlock>("sb-volume", 1s, 44),
    std::make_unique<NetworkBlock>(),
    // TODO: NvidiaGpuBlock
    // TODO: PipewireBlock
    // TODO: InternetBlock
};
// clang-format on

} // namespace config
