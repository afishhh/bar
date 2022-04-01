#pragma once

#include <X11/Xlib.h>
#include <chrono>
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
  bool _charging;
  size_t _charging_gradient_offset;

public:
  BatteryBlock(std::filesystem::path);
  ~BatteryBlock();

  static std::optional<BatteryBlock> find_first();

  size_t draw(Draw &) override;
  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::milliseconds(1000);
  }
};
