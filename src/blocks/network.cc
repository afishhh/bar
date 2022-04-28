#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

#include "../util.hh"
#include "network.hh"

void NetworkBlock::update() {
  _tx_bytes = 0, _rx_bytes = 0;
  for (auto &n : std::filesystem::directory_iterator("/sys/class/net/")) {
    if (!n.path().filename().string().starts_with("e") &&
        !n.path().filename().string().starts_with("w"))
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
}

size_t NetworkBlock::draw(Draw &draw, std::chrono::duration<double>) {
  auto x = 0;
  x += draw.text(x, draw.vcenter(), "Tx");
  x += 3;

  auto pad_left = [](std::string s, std::size_t count) {
    using ssize = std::make_signed<std::size_t>::type;
    s.insert(0, std::max(ssize(count) - ssize(s.size()), ssize(0)), ' ');
    return s;
  };

  x += draw.text(x, draw.vcenter(),
                 pad_left(to_sensible_unit(_tx_bytes * 2), 8));
  x += draw.text(x, draw.vcenter(), "/s");
  x += 7;
  x += draw.text(x, draw.vcenter(), "Rx");
  x += 3;
  x += draw.text(x, draw.vcenter(),
                 pad_left(to_sensible_unit(_rx_bytes * 2), 8));
  x += draw.text(x, draw.vcenter(), "/s");
  return x;
}
