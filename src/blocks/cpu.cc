#include <algorithm>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

#include <fmt/core.h>

#include "../util.hh"
#include "cpu.hh"

CpuBlock::CpuBlock(Config config) : _config(config) { this->_current = this->read_cpu_times(); }
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
  std::unique_lock lg(_update_mutex);

  this->_previous = this->_current;
  this->_current = this->read_cpu_times();

  _diff = this->_current - this->_previous;

  if (_config.thermal_zone_type) {
    for (auto entry : std::filesystem::directory_iterator("/sys/class/thermal/")) {
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
        bool operator()(const ThermalInfo::TripPoint &a, const ThermalInfo::TripPoint &b) const {
          return a.temperature < b.temperature;
        }
      };

      auto point_range =
          std::ranges::subrange(
              std::filesystem::directory_iterator(entry.path()),
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
          std::views::transform([](const auto &path) {
            ThermalInfo::TripPoint point;

            std::ifstream(path + "_temp") >> point.temperature;
            std::ifstream(path + "_type") >> point.type;
            std::ifstream(path + "_hyst") >> point.hyst;

            return point;
          });

      std::set<ThermalInfo::TripPoint, trip_point_temperature_less> points;
      for (auto point : point_range)
        points.emplace(std::move(point));

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

size_t CpuBlock::draw(ui::draw &draw, std::chrono::duration<double>) {
  std::unique_lock lg(_update_mutex);

  size_t y = draw.vcenter();
  size_t x = 0;
  auto percentage = 100.0 * _diff.total.busy() / _diff.total.total();

  x += draw.text(x, y, _config.prefix, _config.prefix_color);
  x += draw.text(x, y, fmt::format("{:>5.1f}%", percentage));

  if (_thermal) {
    x += draw.textw(" ");

    color color = 0xFFFFFF;
    if (_thermal)
      if (auto const &point = _thermal->current_trip_point)
        color = point->color_by_type();

    x += draw.text(x, y, fmt::format("{:.1f}°C", _thermal->temperature / 1000.), color);
  }

  x += 5;

  for (size_t i = 0; i < _diff.percore.size(); ++i) {
    auto left = x;
    auto width = 8;
    auto top = 3;
    auto bottom = draw.height() - 5;
    auto height = bottom - top;
    x += width;

    auto maxfill = height - 1;
    size_t fill = maxfill * (double)_diff.percore[i].busy() / _diff.percore[i].total();

    auto hue = map_range(_diff.percore[i].busy(), 0, _diff.percore[i].total(), 120, 0);
    color color = color::hsl(map_range(hue, 0, 360, 0, 1), 1, 0.5);
    draw.frect(left, top + (maxfill - fill) + 1, width + 1, fill, color);

    draw.hrect(left, top, width, height);

  }

  return x;
}

void CpuBlock::draw_tooltip(ui::draw &draw, std::chrono::duration<double>, unsigned width) const {
  std::unique_lock lg(const_cast<std::mutex&>(_update_mutex));

  unsigned const bar_width = 100;

  auto draw_one = [this, &draw, width](std::string_view title, Times const &times, unsigned yoff, bool all) {
    draw.text(0, 12 + yoff, title);

    size_t fill = times.total() == 0 ? 0 : bar_width * times.busy() / times.total();
    auto hue = map_range(times.busy(), 0, times.total(), 120, 0);
    color color = color::hsl(map_range(hue, 0, 360, 0, 1), 1, 0.5);
    draw.frect(width - bar_width, 3 + yoff, fill, 16, color);
    draw.hrect(width - bar_width, 3 + yoff, bar_width, 16);

    auto percentage = times.total() == 0 ? 0 : 100.0 * times.busy() / times.total();
    auto ptext = fmt::format("{:.1f}%", percentage);
    auto ptextw = draw.textw(ptext);

    auto x = width - bar_width - 4 - ptextw;
    draw.text(x, 12 + yoff, ptext);

    if (all && _thermal) {
      auto ttext = fmt::format("{:.1f}°C", _thermal->temperature / 1000.);
      auto ttextw = draw.textw(ttext);

      draw.text(x - ttextw - 8, 12 + yoff, ttext);

      if (auto const &point = _thermal->current_trip_point) {
        auto const &tptext = fmt::format("{} thermal point", point->type);
        auto tptextw = draw.textw(tptext);
        draw.text(width / 2 - tptextw / 2, 32 + yoff, tptext, point->color_by_type());
      }
    }
  };

  draw_one("ALL", _diff.total, 0, true);

  auto tpoff = (_thermal && _thermal->current_trip_point) * 10;

  for (unsigned core = 0; core < _diff.percore.size(); ++core)
    draw_one(fmt::format("CORE {}", core), _diff.percore[core], 10 + tpoff + 20 * (core + 1), false);

  auto yoff = 40 + tpoff + 20 * _diff.percore.size();
  {
    draw.text(0, 12 + yoff, fmt::format("SYSTEM: {:.1f}%", 100.0 * _diff.total.system / _diff.total.total()));
    auto rtext = fmt::format("IOWAIT: {:.1f}%", 100.0 * _diff.total.iowait / _diff.total.total());
    draw.text(width - draw.textw(rtext), 12 + yoff, rtext);
  }

  yoff += 20;
  {
    draw.text(0, 12 + yoff, fmt::format("USER {:.1f}%", 100.0 * _diff.total.user / _diff.total.total()));
    auto rtext = fmt::format("IDLE: {:.1f}%", 100.0 * _diff.total.idle / _diff.total.total());
    draw.text(width - draw.textw(rtext), 12 + yoff, rtext);
  }
}
