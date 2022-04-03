#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <vector>

#include "cpu.hh"

CpuBlock::CpuBlock(Config config) : _config(config) {}
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
  if (_initialised) {
    this->_previous = this->_current;
    this->_current = this->read_cpu_times();
  } else {
    this->_current = this->read_cpu_times();
    // Zero out this->_previous
    std::memset(&this->_previous.total, 0, sizeof(this->_previous.total));
    this->_previous.percore.resize(this->_current.percore.size());
    for (auto &t : this->_previous.percore) {
      std::memset(&t, 0, sizeof(t));
    }

    this->_initialised = true;
  }

  _diff = this->_current - this->_previous;

  // Thermals
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

      std::vector<ThermalInfo::TripPoint> points;
      // FIXME: Hacky and for some reason adds each point thrice
      for (auto subentry : std::filesystem::directory_iterator(entry.path())) {
        if (!subentry.path().filename().string().starts_with("trip_point_") &&
            !subentry.path().filename().string().ends_with("_type"))
          continue;

        // Get the number of the trip point frmo the path
        size_t trip_point_number =
            std::stoul(subentry.path().filename().string().substr(
                std::string_view("trip_point_").size()));
        ThermalInfo::TripPoint point;

        {
          auto temp_path =
              entry.path() /
              ("trip_point_" + std::to_string(trip_point_number) + "_temp");
          std::ifstream temp_file(temp_path);
          temp_file >> point.temperature;
        }

        {
          auto type_path =
              entry.path() /
              ("trip_point_" + std::to_string(trip_point_number) + "_type");
          std::ifstream type_file(type_path);
          type_file >> point.type;
        }

        {
          auto hyst_path =
              entry.path() /
              ("trip_point_" + std::to_string(trip_point_number) + "_hyst");
          std::ifstream hyst_file(hyst_path);
          hyst_file >> point.hyst;
        }

        points.push_back(point);
      }

      std::sort(points.begin(), points.end(), [](const auto &a, const auto &b) {
        return a.temperature > b.temperature;
      });

      for (auto &point : points) {
        if (point.temperature < _thermal->temperature) {
          _thermal->current_trip_point = point;
          break;
        }
      }
    }
  }
}

size_t CpuBlock::draw(Draw &draw, std::chrono::duration<double> delta) {
  size_t y = draw.vcenter();
  size_t x = 0;
  auto percentage = 100.0 * _diff.total.busy() / _diff.total.total();

  std::stringstream ss;
  ss << std::setw(5) << std::right << std::fixed << std::setprecision(1)
     << std::fixed << percentage << "%";

  x += draw.text(x, y, "CPU: ");
  x += draw.text(x, y, ss.str());

  if (_thermal) {
    x += 5;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1)
       << (double)_thermal->temperature / 1000 << "°C";

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

    x += draw.text(x, y, ss.str(), color);
  }

  x += 5;

  for (size_t i = 0; i < _diff.percore.size(); ++i) {
    auto left = x;
    auto right = x += 8;
    auto top = 0;
    auto bottom = draw.height() - 1;

    draw.line(left, top, right, top);
    draw.line(left, bottom, right, bottom);
    draw.line(left, top, left, bottom);
    draw.line(right, top, right, bottom);

    auto maxfill = bottom - top - 1;
    double percentage =
        (double)_diff.percore[i].busy() / _diff.percore[i].total();
    size_t fill = maxfill * percentage;

    unsigned long color;
    if (percentage > 0.80)
      color = 0xFF0000;
    else if (percentage > 0.60)
      color = 0xFFA500;
    else if (percentage > 0.40)
      color = 0xFFFF00;
    else if (percentage > 0.20)
      color = 0x00FF00;
    else
      color = 0x00FFFF;

    draw.rect(left + 1, top + (maxfill - fill) + 1, right - left - 1, fill,
              color);
  }

  return x;
}
