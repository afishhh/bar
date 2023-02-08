#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "../block.hh"
#include "../ui/x11/window.hh"

class XSystrayBlock : public Block {
  std::optional<ui::x11::XWinID> _tray{};
  std::map<ui::x11::XWinID, ui::x11::embedder> _icons;

public:
  XSystrayBlock() {}

  void relayout_tray();

  void late_init() override;
  std::size_t draw(ui::draw &, std::chrono::duration<double>) override {
    return 0;
  }
  std::size_t ddraw(ui::draw &, std::chrono::duration<double> delta, size_t x,
                    bool right_aligned) override;
};
