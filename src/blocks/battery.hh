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
  size_t _charge_full, _charge_full_design, _charge_now;
  size_t _current_now, _voltage_now;
  bool _charging;
  size_t _charging_gradient_offset;

public:
  struct Config {
    bool show_percentage;
    bool show_time_left_charging;
    bool show_time_left_discharging;
    unsigned short time_precision;
    size_t bar_width;
    bool show_wattage;
    bool show_degradation;
  } _config;

  BatteryBlock(std::filesystem::path, Config config);
  ~BatteryBlock();

  static std::optional<BatteryBlock> find_first(Config config);

  size_t draw(Draw &) override;
  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::milliseconds(1000);
  }
};
