#pragma once

#include <X11/X.h>
#include <cstddef>
#include <map>
#include <set>
#include <vector>

#include "../block.hh"
#include "../draw.hh"

class XSystrayBlock : public Block {
  Window _tray{0};
  std::size_t _icon_count;

  std::set<Window> _icons;
  // HACK: Hack until a proper XEmbed API is available in XWindowBackend
  std::map<Window, EventLoop::callback_id> _icon_embed_callbacks;

public:
  XSystrayBlock() {}

  void late_init() override;
  std::size_t draw(Draw &, std::chrono::duration<double>) override { return 0; }
  std::size_t ddraw(Draw &, std::chrono::duration<double> delta, size_t x,
                    bool right_aligned) override;
};
