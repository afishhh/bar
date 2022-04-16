#include "dwm.hh"

#include "dwmipcpp/connection.hpp"
#include "dwmipcpp/errors.hpp"
#include "dwmipcpp/types.hpp"
#include "dwmipcpp/util.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

DwmBlock::DwmBlock(const std::filesystem::path &socket_path)
    : _connection(dwmipc::Connection(socket_path)) {}

void DwmBlock::late_init() {
  // FIXME: I'm pretty sure this doesn't handle multiple monitors properly.

  std::shared_ptr<std::vector<dwmipc::Monitor>> monitors;
  auto find_selected_monitor = [&]() -> const dwmipc::Monitor & {
    monitors = _connection.get_monitors();
    for (const auto &mon : *monitors) {
      if (mon.is_selected)
        return mon;
    }
    throw std::runtime_error("No bar monitor found");
  };

  auto bar_monitor = find_selected_monitor();
  {
    auto tags = _connection.get_tags();
    for (const auto &tag : *tags) {
      _tags.push_back(Tag{
          // HACK: You might ask, why's this shit here?
          //       and I can only tell you, I don't fucking know.
          .name = tag.tag_name == "" ? "1" : tag.tag_name,
          .bitmask = tag.bit_mask,
          .selected = bool(bar_monitor.tag_state.selected & tag.bit_mask),
          .occupied = bool(bar_monitor.tag_state.occupied & tag.bit_mask),
          .urgent = bool(bar_monitor.tag_state.urgent & tag.bit_mask),
      });
    }
  }

  auto update_tag_states = [&](const dwmipc::TagState &state) {
    for (auto &tag : _tags) {
      tag.selected = state.selected & tag.bitmask;
      tag.occupied = state.occupied & tag.bitmask;
      tag.urgent = state.urgent & tag.bitmask;
    }
  };

  for (const auto &mon : *monitors) {
    if (mon.is_selected) {
      if (mon.clients.selected == 0)
        _focused_client_title = "";
      else
        _focused_client_title =
            _connection.get_client(mon.clients.selected)->name;
      _layout_symbol = mon.layout.symbol.cur;
    }
  }

  _connection.on_tag_change =
      [update_tag_states](const dwmipc::TagChangeEvent &event) {
        update_tag_states(event.new_state);
      };
  _connection.subscribe(dwmipc::Event::TAG_CHANGE);
  _connection.on_client_focus_change =
      [this](const dwmipc::ClientFocusChangeEvent &event) {
        try {
          if (event.new_win_id == 0)
            _focused_client_title = "";
          else
            _focused_client_title =
                _connection.get_client(event.new_win_id)->name;
        } catch (dwmipc::ResultFailureError &err) {
          std::cerr << "get_client(ClientFocusChangeEvent->client) failed: "
                    << err.what() << '\n';
        }
      };
  _connection.subscribe(dwmipc::Event::CLIENT_FOCUS_CHANGE);
  _connection.on_focused_title_change =
      [this](const dwmipc::FocusedTitleChangeEvent &event) {
        _focused_client_title = event.new_name;
      };
  _connection.subscribe(dwmipc::Event::FOCUSED_TITLE_CHANGE);
  _connection.on_layout_change =
      [this](const dwmipc::LayoutChangeEvent &event) {
        _layout_symbol = event.new_symbol;
      };
  _connection.subscribe(dwmipc::Event::LAYOUT_CHANGE);
}
DwmBlock::~DwmBlock() {}

size_t DwmBlock::draw(Draw &draw, std::chrono::duration<double>) {
  size_t x = 0;
  for (const auto &tag : _tags) {
    if (!tag.occupied && !tag.selected && !tag.urgent)
      continue;
    x +=
        draw.text(x, draw.vcenter(), tag.name,
                  tag.urgent ? 0xFF0000 : (tag.selected ? 0x00FF00 : 0xFFFFFF));
    x += 7;
  }

  x += draw.text(x, draw.vcenter(), _layout_symbol) + 7;

  // Limit name to 40 chars
  auto trunc =
      std::min(_focused_client_title.begin() + 40, _focused_client_title.end());
  auto w = draw.text(x, draw.vcenter(),
                     std::string_view(_focused_client_title.begin(), trunc));
  std::string xs(40, 'x');
  x += std::max(w, draw.textw(xs));
  return x;
}
void DwmBlock::update() { _connection.handle_event(); }
