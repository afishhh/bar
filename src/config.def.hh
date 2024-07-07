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
#include <string_view>

using namespace std::literals;

namespace config {

// The height of the status bar, note that internally the bar's coordinate system will always place 0 at the top and 24 at the bottom.
constexpr static size_t height = 24;

// The order in which different platforms are attempted.
constexpr int init_platform_order[] = {GLFW_PLATFORM_WAYLAND, GLFW_ANY_PLATFORM};

constexpr color background_color = color::rgb(0, 0, 120);

// Configuration options specific to the X11 backend
namespace x11 {

// Why? Fractional scaling. TODO: explain why
static const size_t height = 48;
// Whether to set the override-redirect flag on the bar window.
constexpr static bool override_redirect = false;
// The window name of the bar window.
constexpr static std::string_view window_name = "bar";
// The class name of the bar window.
// NOTE: I am not sure if this can be const because XClassHint properties aren't
//       but don't seem to be modified anyway.
constexpr static std::string_view window_class = "bar";

} // namespace x11

constexpr static std::initializer_list<const char *> fonts = {
    "monospace:size=10",
    "Font Awesome 6 Free Solid:style=Solid:size=9"
};

// clang-format off
const static std::unique_ptr<Block> left_blocks[] = {
    std::make_unique<DwmBlock>(
        DwmBlock::Config {
            .socket_path = "/tmp/dwm.socket",
            .show_empty_tags = false,
            .inactive_tag_color = 0xFFFFFF,
            .selected_tag_color = 0x00FF00,
            .urgent_tag_color = 0xFF0000,
            .empty_tag_color = 0xFFFFFF,

            .floating_title_prefix = " ",
            .title_color = 0xFFFFFF,
            .floating_title_color = 0xFFFFFF,

            .max_title_length = 100,
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
            // If the title is not set then it's set to the mountpoint path
            // .title = "root",
            .show_fs_type = false,
            .show_usage_text = true,
            .usage_text_in_bar = false,
            .show_usage_bar = true,
            .bar_width = 45,
            // If the color is not set then it's chosen based on the usage percentage.
            // .bar_fill_color = 0x00FF00,
        }
    ),
    std::make_unique<ScriptBlock>(ScriptBlock::Config {
        .path = "sb-mic-volume",
        .interval = 1s,
        // The block will be updated if any of the signals in this vector are received.
        .update_signals{SIGRTMIN + 20}
    }),
    std::make_unique<ScriptBlock>(ScriptBlock::Config {
        .path = "sb-volume",
        .interval = 1s,
        .update_signals{44},
        // If this flag is set (default) then if the script output would result in an empty block then the block is instead just skipped.
        // .skip_on_empty = false
    }),
    std::make_unique<NetworkBlock>(),
};
// clang-format on

} // namespace config
