#pragma once

#include <X11/X.h>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "../block.hh"

class XSystrayBlock : public Block {
  std::optional<Window> _tray{};
  std::map<Window, EventLoop::callback_id> _icons;

public:
  XSystrayBlock() {}

  void relayout_tray();

  bool skip() override;

  void late_init() override;
  std::size_t draw(ui::draw &, std::chrono::duration<double>) override {
    return 0;
  }
  std::size_t ddraw(ui::draw &, std::chrono::duration<double> delta, size_t x,
                    bool right_aligned) override;
};
