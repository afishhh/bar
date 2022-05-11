#include <algorithm>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <ranges>
#include <sstream>
#include <string_view>
#include <vector>

#include "../format.hh"
#include "../util.hh"
#include "cpu.hh"

CpuBlock::CpuBlock(Config config) : _config(config) {
  this->_current = this->read_cpu_times();
}
CpuBlock::~CpuBlock() {}

CpuBlock::AllTimes CpuBlock::read_cpu_times() {
  std::ifstream stat("/proc/stat");

  AllTimes all;
  std::string name;
  stat >> name;
  stat >> all.total.user;
  stat >> all.total.nice;
  stat >> all.total.system;
  stat >> all.total.idle;
  stat >> all.total.iowait;
  stat >> all.total.irq;
  stat >> all.total.softirq;
  stat >> all.total.steal;
  stat >> all.total.guest;
  stat >> all.total.guest_nice;

  while (stat >> name && name.starts_with("cpu")) {
    Times t;
    stat >> t.user;
    stat >> t.nice;
    stat >> t.system;
    stat >> t.idle;
    stat >> t.iowait;
    stat >> t.irq;
    stat >> t.softirq;
    stat >> t.steal;
    stat >> t.guest;
    stat >> t.guest_nice;
    all.percore.push_back(std::move(t));
  }

  return all;
}

void CpuBlock::update() {
  this->_previous = this->_current;
  this->_current = this->read_cpu_times();

  _diff = this->_current - this->_previous;

  if (_config.thermal_zone_type) {
    for (auto entry :
         std::filesystem::directory_iterator("/sys/class/thermal/")) {
      if (!entry.path().filename().string().starts_with("thermal_zone"))
        continue;

      {
        auto type_path = entry.path() / "type";
        std::ifstream type_file(type_path);
        std::string type;
        type_file >> type;

        if (type != _config.thermal_zone_type)
          continue;
      }

      _thermal = ThermalInfo();
      {
        auto temp_path = entry.path() / "temp";
        std::ifstream temp_file(temp_path);
        temp_file >> _thermal->temperature;
      }

      class trip_point_temperature_less {
      public:
        bool operator()(const ThermalInfo::TripPoint &a,
                        const ThermalInfo::TripPoint &b) const {
          return a.temperature < b.temperature;
        }
      };
      std::set<ThermalInfo::TripPoint, trip_point_temperature_less> points;
      std::ranges::subrange(std::filesystem::directory_iterator(entry.path()),
                            std::filesystem::directory_iterator()) |
          std::views::filter([](const auto &entry) {
            auto n = entry.path().filename().string();
            return n.starts_with("trip_point_") && n.ends_with("_type");
          }) |
          std::views::transform([](const auto &entry) {
            auto f = entry.path().string();
            // remove _type
            return f.erase(f.size() - 5);
          }) |
          std::views::transform([&points](const auto &path) {
            ThermalInfo::TripPoint point;

            std::ifstream(path + "_temp") >> point.temperature;
            std::ifstream(path + "_type") >> point.type;
            std::ifstream(path + "_hyst") >> point.hyst;

            points.insert(std::move(point));
            return 0;
          });

      auto it = std::ranges::find_if(points, [&](const auto &point) {
        return point.temperature < _thermal->temperature;
      });
      if (it != points.end())
        _thermal->current_trip_point = *it;
      else
        _thermal->current_trip_point = std::nullopt;
    }
  }
}

size_t CpuBlock::draw(Draw &draw, std::chrono::duration<double>) {
  size_t y = draw.vcenter();
  size_t x = 0;
  auto percentage = 100.0 * _diff.total.busy() / _diff.total.total();

  x += draw.text(x, y, _config.prefix, _config.prefix_color);
  x += draw.text(x, y, std::format("{:>5.1f}%", percentage));

  if (_thermal) {
    x += draw.textw(" ");

    auto color = 0xFFFFFF;
    // Feels hacky but whatever :)
    if (auto &point = _thermal->current_trip_point) {
      if (point->type == "critical")
        color = 0xFF0000;
      else if (point->type == "hot")
        color = 0xFF8800;
      else if (point->type == "warm")
        color = 0xFFFF00;
      else if (point->type == "passive")
        color = 0x00FF00;
      else if (point->type == "active")
        color = 0x00FFFF;
      else if (point->type == "off")
        color = 0xCCCCCC;
    }

    x += draw.text(x, y, std::format("{:.1f}°C", _thermal->temperature / 1000.),
                   color);
  }

  x += 5;

  for (size_t i = 0; i < _diff.percore.size(); ++i) {
    auto left = x;
    auto width = 8;
    auto top = 3;
    auto bottom = draw.height() - 6;
    auto height = bottom - top;
    x += width;

    draw.hrect(left, top, width, height);

    auto maxfill = height - 1;
    size_t fill =
        maxfill * (double)_diff.percore[i].busy() / _diff.percore[i].total();

    auto hue =
        map_range(_diff.percore[i].busy(), 0, _diff.percore[i].total(), 120, 0);
    unsigned long color =
        rgb_to_long(hsl_to_rgb(map_range(hue, 0, 360, 0, 1), 1, 0.5));

    draw.frect(left + 1, top + (maxfill - fill) + 1, width - 1, fill, color);
  }

  return x;
}
