#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <vector>

#include "../block.hh"
#include "../ui/x11/window.hh"

class XSystrayBlock : public Block {
  std::optional<ui::x11::XWinID> _tray{};
  std::size_t _icon_count;

  std::set<ui::x11::XWinID> _icons;
  // HACK: Hack until a proper XEmbed API is available in XWindowBackend
  std::map<ui::x11::XWinID, EventLoop::callback_id> _icon_embed_callbacks;

public:
  XSystrayBlock() {}

  void late_init() override;
  std::size_t draw(ui::draw &, std::chrono::duration<double>) override {
    return 0;
  }
  std::size_t ddraw(ui::draw &, std::chrono::duration<double> delta, size_t x,
                    bool right_aligned) override;
};
