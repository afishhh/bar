#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

#include "../format.hh"
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

size_t NetworkBlock::draw(ui::draw &draw, std::chrono::duration<double>) {
  return draw.text(0, std::format("Tx {: >8}/s Rx {: >8}/s",
                                  to_sensible_unit(_tx_bytes, 1),
                                  to_sensible_unit(_rx_bytes, 1)));
}
