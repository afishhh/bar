#include <atomic>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <spawn.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <unordered_map>
#include <utility>

#include <fmt/core.h>

#include "../log.hh"
#include "../util.hh"
#include "network.hh"

auto const DEVICE_CLASS_ETHERNET = 0x020000;
auto const DEVICE_CLASS_WIFI = 0x028000;

void iwctl_parse_output(std::string const &output, IwctlStationInfo &info) {
  std::istringstream out(output);
  std::unordered_map<std::string, std::string> properties;

  for (uint64_t i = 0; i < 4; ++i)
    out.ignore(std::numeric_limits<std::streamsize>().max(), out.widen('\n'));
  out.ignore(std::numeric_limits<std::streamsize>().max(), out.widen(' '));

  std::string k, v;
  while (out >> k) {
    while (true) {
      // this will be a space character
      out.get();
      // if this is not a space character then the previous space was just a
      // space in the property name
      if (!out)
        throw std::runtime_error("could not parse iwctl output");
      else if (out.peek() != ' ') {
        k += ' ';
        // FIXME: don't do this
        std::string next;
        out >> next;
        k += next;
      } else
        break;
    }

    std::getline(out, v);
    v = trim(v);

    properties.emplace(std::move(k), std::move(v));
  }

  info.scanning = properties["Scanning"] == "yes";
  if (properties["State"] == "connected") {
    auto _pop_prop = [&properties]<typename T>(std::string const &prop) {
      auto it = properties.find(prop);
      if (it == properties.end()) {
        warn << "iwctl network missing property " << prop << '\n';
        return T();
      }
      auto value = std::move(it->second);
      properties.erase(it);

      if constexpr (std::same_as<std::string, T>) {
        return value;
      } else {
        T out{0};
        auto result = std::from_chars(value.c_str(), value.c_str() + value.size(), out);
        if (result.ec != std::errc())
          warn << "from_chars failed on iwctl property " << prop << " (" << properties[prop] << ")" << '\n';
        return out;
      }
    };

#define pop_prop(tp, prop) _pop_prop.template operator()<tp>(prop)

    info.connection = IwctlConnectionInfo{.connected_network = pop_prop(std::string, "Connected network"),
                                          .ipv4_address = properties.contains("IPv4 address")
                                                              ? std::optional(pop_prop(std::string, "IPv4 address"))
                                                              : std::nullopt,
                                          .connected_bss = pop_prop(std::string, "ConnectedBss"),
                                          .security = pop_prop(std::string, "Security"),
                                          .frequency = pop_prop(unsigned, "Frequency"),

                                          .rssi = pop_prop(signed, "RSSI"),
                                          .average_rssi = pop_prop(signed, "AverageRSSI"),

                                          .others = std::move(properties)};

#undef pop_prop
  }
}

void iwctl_update_station(WifiStation &station) {
  run(&station.iwctl_process, {"iwctl", "station", station.name, "show"}, {NULL},
      [&station](int status, int signal, std::string output) {
        if (status || signal) {
          if (signal)
            station.iwctl_status = -1;
          else
            station.iwctl_status = status;
          station.info.reset();
        }

        IwctlStationInfo new_info;
        try {
          iwctl_parse_output(output, new_info);
        } catch (std::exception const &ex) {
          warn << "failed to parse iwd output for " << station.name << ": " << ex.what() << '\n';
          station.update_running.clear(std::memory_order_release);
          return;
        }

        {
          std::unique_lock lock(station.modify_mutex);
          station.iwctl_status = 0;
          station.info = std::move(new_info);
        }

        station.update_running.clear(std::memory_order_release);
      });
}

NetworkBlock::Config NetworkBlock::Config::autodetect() {
  Config out;

  for (auto entry : std::filesystem::directory_iterator("/sys/class/net/")) {
    if (!std::filesystem::exists(entry.path() / "device"))
      continue;

    uint64_t device_class;
    std::ifstream(entry.path() / "device" / "class") >> std::hex >> device_class;

    if (device_class == DEVICE_CLASS_ETHERNET)
      out._ethernet_devices.push_back(entry.path().filename().string());
    else if (device_class == DEVICE_CLASS_WIFI)
      out._wifi_devices.push_back(entry.path().filename().string());
  }

  return out;
}

void NetworkBlock::update() {
  _tx_bytes = 0, _rx_bytes = 0;
  for (auto &n : std::filesystem::directory_iterator("/sys/class/net/")) {
    if (!std::filesystem::exists(n.path() / "device"))
      continue;
    auto tx = n.path() / "statistics" / "tx_bytes";
    auto rx = n.path() / "statistics" / "rx_bytes";
    auto read = [](std::filesystem::path p) {
      std::ifstream ifs(p);
      if (!ifs.is_open())
        return 0ul;
      size_t a;
      ifs >> a >> a;
      return a;
    };
    if (std::filesystem::exists(tx))
      _tx_bytes += read(tx);
    if (std::filesystem::exists(rx))
      _rx_bytes += read(rx);
  }
  auto old_tx = _tx_bytes, old_rx = _rx_bytes;

  _tx_bytes -= _last_tx_bytes;
  _rx_bytes -= _last_rx_bytes;

  _last_tx_bytes = old_tx;
  _last_rx_bytes = old_rx;

  for (auto const &name : _config._ethernet_devices) {
    std::ifstream(std::filesystem::path("/sys/class/net/") / name / "carrier") >> _ethernet_connected;
    if (_ethernet_connected)
      break;
  }

  for (auto &station : _wifi_stations) {
    if (!station.update_running.test_and_set(std::memory_order_acq_rel))
      iwctl_update_station(station);
  }
}

size_t NetworkBlock::draw(ui::draw &draw, std::chrono::duration<double>) {
  auto x = 0;

  if (_ethernet_connected)
    x += draw.text(x, "");

  for (auto const &station : _wifi_stations) {
    std::unique_lock lock(station.modify_mutex);
    if (station.info) {
      if ((station.info->connection || station.info->scanning) && (x != 0 || _ethernet_connected))
        x += 10;
      if (station.info->connection)
        x += draw.text(x, " ");
      if (station.info->scanning) {
        x += draw.text(x, "󰍉");
        if (!station.info->connection)
          x += draw.text(x, station.name);
      }
      if (station.info->connection)
        x += draw.text(x, station.info->connection->connected_network);
    } else if (station.iwctl_status) {
      x += draw.text(x, fmt::format(" {} unknown", station.name), color(0xFF0000));
    }
  }

  if (x == 0)
    x += draw.text(x, "Not connected");

  return x;
}

void NetworkBlock::draw_tooltip(ui::draw &draw, std::chrono::duration<double>, unsigned int) const {
  ui::draw::pos_t width = 250;
  draw.line(0, 0, width, 0, 0);

  auto y = 0;
  for (auto const &station : _wifi_stations) {
    std::unique_lock lock(station.modify_mutex);

    if (!station.iwctl_status && !station.info->scanning && !station.info->connection)
      continue;

    auto const &t = station.name;
    draw.text((width - draw.textw(t)) / 2, 12 + y, t);

    y += 20;

    if (station.iwctl_status) {
      auto t = fmt::format("'iwctl station {} show' exited with {}", station.name, station.iwctl_status);
      auto w = draw.textw(t);
      ui::draw::pos_t tx = 0;
      if (width < w)
        tx = 0;
      else
        tx = (width - draw.textw(t)) / 2;
      draw.text(tx, 12 + y, t, 0xFF0000);
      y += 20;
    } else {

      if (station.info->scanning) {
        auto t = "Scanning...";
        draw.text((width - draw.textw(t)) / 2, 12 + y, t, 0x8888FF);
        y += 20;
      }

      if (station.info->connection) {
        auto const &conn = station.info->connection;

        unsigned x = 0;
        x += draw.text(x, 12 + y, "Connected to ");
        draw.text(x, 12 + y, conn->connected_network, 0xFFAA00);

        y += 20;

        draw.text(0, 12 + y, fmt::format("RSSI: {}", conn->rssi));

        y += 20;

        draw.text(0, 12 + y, fmt::format("Security: {}", conn->security));

        y += 20;

        std::string_view ip = "Unknown";
        if (conn->ipv4_address)
          ip = conn->ipv4_address.value();
        draw.text(0, 12 + y, fmt::format("IP: {}", ip));

        y += 20;
      }
    }
  }

  if (y)
    y += 10;

  draw.text(0, 12 + y, fmt::format("Rx {: >8}/s", to_sensible_unit(_rx_bytes, 1)));
  auto t = fmt::format("Tx {: >8}/s", to_sensible_unit(_tx_bytes, 1));
  draw.text((width - draw.textw(t)), 12 + y, t);
}
