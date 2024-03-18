#pragma once

#include <X11/Xlib.h>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>

#include "../block.hh"

// Fuck xlib for defining a "Status" macro
enum class BatteryStatus {
  Charging,
  Discharging,
  Full,
  NotCharging,
  Unknown,
};

class BatteryBlock : public Block {
private:
  std::filesystem::path _path;
  // size_t _charge_full, _charge_full_design, _charge_now;
  // size_t _current_now, _voltage_now;
  double _charge_level, _max_charge_level, _wattage_now, _degradation;
  size_t _seconds_left;
  bool _charging, _full;
  size_t _charging_gradient_offset;

public:
  struct Config {
    std::string prefix{};
    color prefix_color = 0xFFFFFF;
    bool show_percentage = true;
    bool show_time_left_charging = true;
    bool show_time_left_discharging = true;
    unsigned short time_precision = -1;
    size_t bar_width = 70;
    bool show_wattage = true;
    bool show_degradation = false;
  };

private:
  Config _config;

public:
  BatteryBlock(std::filesystem::path, Config config);
  ~BatteryBlock();

  static std::optional<BatteryBlock> find_first(Config config);

  size_t draw(ui::draw &, std::chrono::duration<double> delta) override;

  void animate(EventLoop::duration delta) override;
  std::optional<EventLoop::duration> animate_interval() override {
    return std::chrono::milliseconds(50);
  }
  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::milliseconds(1000);
  }
};
