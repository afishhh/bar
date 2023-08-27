#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <forward_list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../block.hh"

struct IwctlConnectionInfo {
  std::string connected_network;
  std::optional<std::string> ipv4_address;
  std::string connected_bss;
  std::string security;
  unsigned frequency;

  signed rssi;
  signed average_rssi;

  std::unordered_map<std::string, std::string> others;
};

struct IwctlStationInfo {
  bool scanning;
  std::optional<IwctlConnectionInfo> connection;
};

struct WifiStation {
  std::string name;
  mutable std::mutex modify_mutex;
  std::atomic_bool update_running;
  IwctlStationInfo info;

  WifiStation(std::string const &name) : name(name) {}
};

// iwctl only for now
class NetworkBlock : public Block {
  size_t _last_tx_bytes = 0, _last_rx_bytes = 0;
  size_t _tx_bytes, _rx_bytes;

  bool _ethernet_connected;
  std::forward_list<WifiStation> _wifi_stations;

  struct Config {
    std::vector<std::string> _ethernet_devices;
    std::vector<std::string> _wifi_devices;

    static Config autodetect();
  };

private:
  Config _config;

public:
  NetworkBlock() : NetworkBlock(Config::autodetect()) {}
  NetworkBlock(Config config) : _config(std::move(config)) {
    for (auto const &name : _config._wifi_devices)
      _wifi_stations.emplace_front(name);
  }
  ~NetworkBlock() {}

  size_t draw(ui::draw &, std::chrono::duration<double> delta) override;
  void update() override;
  std::chrono::duration<double> update_interval() override {
    return std::chrono::milliseconds(500);
  }

  bool has_tooltip() const override { return true; }
  void draw_tooltip(ui::draw &, std::chrono::duration<double>,
                    unsigned int) const override;
};
