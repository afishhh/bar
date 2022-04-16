#pragma once

#include <cassert>
#include <optional>
#include <vector>

#include "../block.hh"

class CpuBlock : public Block {
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

    long busy() const {
      return user + nice + system + irq + softirq + steal + guest + guest_nice;
    }
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
      for (size_t i = 0; i < percore.size(); ++i) {
        result.percore.push_back(percore[i] - other.percore[i]);
      }

      return result;
    }
  };
  AllTimes _previous;
  AllTimes _current;

  AllTimes _diff;

  bool _initialised = false;

  AllTimes read_cpu_times();

  struct ThermalInfo {
    struct TripPoint {
      long hyst;
      long temperature;
      std::string type;
    };
    std::optional<TripPoint> current_trip_point;

    long temperature;
  };
  std::optional<ThermalInfo> _thermal;

public:
  struct Config {
    std::optional<std::string> thermal_zone_type;
  };

private:
  Config _config;

public:
  CpuBlock(Config config);
  ~CpuBlock();

  size_t draw(Draw &, std::chrono::duration<double> delta) override;
  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::milliseconds(500);
  }
};
