#include <chrono>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <optional>
#include <string>

#include <fmt/core.h>

#include "../util.hh"
#include "../log.hh"
#include "battery.hh"

BatteryBlock::BatteryBlock(std::filesystem::path path, BatteryBlock::Config config) : _path(path), _config(config) {}
BatteryBlock::~BatteryBlock() {}

static size_t read_int(std::filesystem::path path) {
  std::ifstream ifs(path);
  if (!ifs.is_open())
    throw std::runtime_error("Could not open file");
  std::string line;
  std::getline(ifs, line);
  // Happens right after unplugging or plugging in the battery.
  if (line.empty())
    return 0;
  return std::stoul(line);
}

static double read_micro(std::filesystem::path path) { return (double)read_int(path) / 1000000.; }

std::optional<BatteryBlock> BatteryBlock::find_first(Config config) {
  for (auto &entry : std::filesystem::directory_iterator("/sys/class/power_supply")) {
    if (entry.path().filename() == "BAT0") {
      return std::make_optional<BatteryBlock>(entry.path(), config);
    }
  }
  return {};
}

void BatteryBlock::update() {
  std::ifstream ifs(_path / "status");
  if (!ifs.is_open())
    throw std::runtime_error("Could not open battery status file");
  std::string line;
  std::getline(ifs, line);
  _charging = line == "Charging";
  _full = line == "Full";

  if (std::filesystem::exists(_path / "charge_now")) {
    // case 1 = charge is available
    size_t charge_now = read_int(_path / "charge_now");
    size_t charge_full = read_int(_path / "charge_full");
    size_t charge_full_design = read_int(_path / "charge_full_design");
    size_t current_now = read_int(_path / "current_now");
    size_t voltage_now = read_int(_path / "voltage_now");

    _charge_level = (double)charge_now / charge_full;
    _wattage_now = (voltage_now / 1000. / 1000.) * (current_now / 1000. / 1000.);

    if (_charging)
      _seconds_left = (double)(charge_full - charge_now) / current_now * 3600;
    else
      _seconds_left = (double)charge_now / current_now * 3600;

    _degradation = (double)charge_full / charge_full_design * 100.;
  } else if (std::filesystem::exists(_path / "energy_now")) {
    // case 2 = energy is available
    double energy_now = read_micro(_path / "energy_now");
    double energy_full = read_micro(_path / "energy_full");
    double energy_full_design = read_micro(_path / "energy_full_design");
    double power_now = read_micro(_path / "power_now");

    _charge_level = energy_now / energy_full;
    _wattage_now = power_now;

    if (_charging)
      _seconds_left = (energy_full - energy_now) / power_now * 3600;
    else
      _seconds_left = energy_now / power_now * 3600;

    _degradation = energy_full / energy_full_design * 100.;
  } else {
    // case 3 = we don't have enough (or don't know where to look)
    // TODO: Display an error
  }

  if (std::filesystem::exists(_path / "charge_control_end_threshold"))
    _max_charge_level = (double)read_int(_path / "charge_control_end_threshold") / 100.;
  else
    _max_charge_level = 1.0;
}

void BatteryBlock::animate(Interval delta) {
  if (_charging)
    _charging_gradient_offset =
        (_charging_gradient_offset + std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count() / 2500000) %
        size_t(map_range(_charge_level, 0, _max_charge_level, 0, (_config.bar_width - 1) * 20));
}

size_t BatteryBlock::draw(ui::draw &draw, std::chrono::duration<double>) {
  double battery_percent = map_range(_charge_level, 0, _max_charge_level, 0, 100);
  size_t x = 0;

  x += draw.text(x, draw.vcenter(), _config.prefix, _config.prefix_color);
  if (_config.show_percentage)
    x += draw.text(x, draw.vcenter(), fmt::format("{:>5.1f}%", battery_percent));

  x += 5;

  if (_config.show_wattage)
    x += draw.text(x, draw.vcenter(), fmt::format("{:>4.1f}W", _wattage_now));

  x += 5;

  auto top = 3;
  auto bottom = draw.height() - 6;
  auto height = bottom - top;
  auto left = x;
  auto width = _config.bar_width;
  x += width;

  draw.hrect(left, top, width, height);

  size_t fill_width = battery_percent / 100 * (width - 1);

  auto format_time = [this](size_t seconds) {
    std::string time_str;
    unsigned short blocks = 0;
    if (seconds >= 24 * 60 * 60 && blocks < _config.time_precision) {
      time_str += std::to_string(seconds / (24 * 60 * 60));
      time_str += "d ";
      seconds %= 24 * 60 * 60;
      ++blocks;
    }
    if (seconds >= 60 * 60 && blocks < _config.time_precision) {
      time_str += std::to_string(seconds / (60 * 60));
      time_str += "h ";
      seconds %= 60 * 60;
      ++blocks;
    }
    if (seconds >= 60 && blocks < _config.time_precision) {
      time_str += std::to_string(seconds / 60);
      time_str += "m ";
      seconds %= 60;
      ++blocks;
    }
    if (blocks < _config.time_precision) {
      time_str += std::to_string(seconds);
      time_str += "s ";
    }

    time_str.pop_back();
    return time_str;
  };

  // If charging then fill the box with a gradient
  if (_charging) {
    for (size_t i = 0; i < fill_width; ++i) {
      auto color = (155 + (unsigned long)((double)i / fill_width * 100)) << 8;
      auto x = left + 1 + ((i + _charging_gradient_offset / 10) % fill_width);
      draw.frect(x, top + 1, 1, bottom - 1 - top, color);
    }

    if (_config.show_time_left_charging && !_full) {
      auto time_left_str = format_time(_seconds_left);
      draw.text(left + width / 2 - draw.textw(time_left_str) / 2, draw.vcenter(), time_left_str);
    }
  } else {
    // 0-100 in HSL is the Red-Green range so we can act as if battery_percent
    // is in the HSL hue range and then just map it to a percentage
    auto hue = map_range(battery_percent, 0, 360, 0, 1);
    color fill_color = color::hsl(hue, .9, .45);

    draw.frect(left + 1, top + 1, fill_width, height - 1, fill_color);

    if (_config.show_time_left_discharging && !_full && _wattage_now > 0) {
      auto time_left_str = format_time(_seconds_left);
      draw.text(left + width / 2 - draw.textw(time_left_str) / 2, draw.vcenter(), time_left_str);
    }
  }

  if (_config.show_degradation) {
    x += 5;
    x += draw.text(x, draw.vcenter(), fmt::format("{:5>.1f}%", _degradation));
  }

  return x;
}
