#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <vector>

#include "../block.hh"

class CpuBlock : public SimpleBlock {
  struct Times {
    long user;
    long nice;
    long system;

    long idle;
    long iowait;

    long irq;
    long softirq;

    long steal;
    long guest;
    long guest_nice;

    long busy() const { return user + nice + system + irq + softirq + steal + guest + guest_nice; }
    long notbusy() const { return idle + iowait; }
    long total() const { return busy() + notbusy(); }

    Times operator-(const Times &other) const {
      Times result;

      result.user = user - other.user;
      result.nice = nice - other.nice;
      result.system = system - other.system;
      result.idle = idle - other.idle;
      result.iowait = iowait - other.iowait;
      result.irq = irq - other.irq;
      result.softirq = softirq - other.softirq;
      result.steal = steal - other.steal;
      result.guest = guest - other.guest;
      result.guest_nice = guest_nice - other.guest_nice;

      return result;
    }
  };

  struct AllTimes {
    Times total;
    std::vector<Times> percore;

    AllTimes operator-(AllTimes &other) {
      AllTimes result;

      result.total = total - other.total;
      assert(percore.size() == other.percore.size());
      for (size_t i = 0; i < percore.size(); ++i)
        result.percore.push_back(percore[i] - other.percore[i]);

      return result;
    }
  };

  AllTimes _previous;
  AllTimes _current;

  AllTimes _diff;

  AllTimes read_cpu_times();

  struct ThermalInfo {
    struct TripPoint {
      long hyst;
      long temperature;
      std::string type;

      color color_by_type() const {
        if (type == "critical")
          return 0xFF0000;
        else if (type == "hot")
          return 0xFF8800;
        else if (type == "warm")
          return 0xFFFF00;
        else if (type == "passive")
          return 0x00FF00;
        else if (type == "active")
          return 0x00FFFF;
        else if (type == "off")
          return 0xCCCCCC;
        else
          return 0xFFFFFF;
      }
    };

    std::optional<TripPoint> current_trip_point;
    long temperature;
  };

  std::optional<ThermalInfo> _thermal;

  std::mutex _update_mutex;

public:
  struct Config {
    std::string prefix;
    color prefix_color = 0xFFFFFF;
    std::optional<std::string> thermal_zone_type;
  };

private:
  Config _config;

public:
  CpuBlock(Config config);
  ~CpuBlock();

  size_t draw(ui::draw &, std::chrono::duration<double> delta) override;
  void update() override;
  Interval update_interval() override { return std::chrono::milliseconds(500); }

  bool has_tooltip() const override { return true; }
  void draw_tooltip(ui::draw &, std::chrono::duration<double>, unsigned) const override;
};
