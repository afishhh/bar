#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "dwmipcpp/connection.hpp"
#include "dwmipcpp/errors.hpp"
#include "dwmipcpp/types.hpp"
#include "dwmipcpp/util.hpp"

#include "dwm.hh"
#include "../log.hh"
#include "../format.hh"

DwmBlock::DwmBlock(const Config &config)
    : _connection(dwmipc::Connection(config.socket_path)), _config(config) {}

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
      if (mon.clients.selected == 0) {
        _focused_client_title = "";
        _focused_client_floating = false;
        _focused_client_urgent = false;
      } else {
        auto c = _connection.get_client(mon.clients.selected);
        _focused_client_title = c->name;
        _focused_client_floating = c->states.is_floating;
        _focused_client_urgent = c->states.is_urgent;
        _layout_symbol = mon.layout.symbol.cur;
      }
    }
  }

  _connection.on_tag_change =
      [update_tag_states](const dwmipc::TagChangeEvent &event) {
        update_tag_states(event.new_state);
      };
  _connection.subscribe(dwmipc::Event::TAG_CHANGE);
  _connection.on_client_focus_change =
      [this](const dwmipc::ClientFocusChangeEvent &event) {
        if (event.new_win_id == 0) {
          _focused_client_title = "";
          _focused_client_floating = false;
          _focused_client_urgent = false;
        } else {
          std::shared_ptr<dwmipc::Client> c{nullptr};
          try {
            c = _connection.get_client(event.new_win_id);
          } catch (dwmipc::ResultFailureError &err) {
            std::print(
                warn, "get_client(ClientFocusChangeEvent->client) failed: {}\n",
                err.what());
          }
          _focused_client_title = c->name;
          _focused_client_floating = c->states.is_floating;
          _focused_client_urgent = c->states.is_urgent;
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
  _connection.on_focused_state_change =
      [this](const dwmipc::FocusedStateChangeEvent &event) {
        _focused_client_floating = event.new_state.is_floating;
        _focused_client_urgent = event.new_state.is_urgent;
      };
  _connection.subscribe(dwmipc::Event::FOCUSED_STATE_CHANGE);
}
DwmBlock::~DwmBlock() {}

size_t DwmBlock::draw(Draw &draw, std::chrono::duration<double>) {
  size_t x = 0;
  for (const auto &tag : _tags) {
    if (!(tag.occupied && !_config.show_empty_tags) && !tag.selected &&
        !tag.urgent)
      continue;
    Draw::color_t color;
    if (tag.urgent)
      color = _config.urgent_tag_color;
    else if (tag.selected)
      color = _config.selected_tag_color;
    else if (tag.occupied)
      color = _config.inactive_tag_color;
    else
      color = _config.empty_tag_color;
    x += draw.text(x, draw.vcenter(), tag.name, color);
    x += 7;
  }

  x += draw.text(x, draw.vcenter(), _layout_symbol) + 7;

  std::string title = _focused_client_floating ? _config.floating_title_prefix +
                                                     _focused_client_title
                                               : _focused_client_title;
  Draw::pos_t width = 0;
  if (_config.max_title_length) {
    auto end =
        std::min(_focused_client_title.begin() + *_config.max_title_length,
                 _focused_client_title.end());
    title = std::string_view(_focused_client_title.begin(), end);
    std::string xs(*_config.max_title_length, 'x');
    width = draw.textw(xs);
  }
  x += std::max(width, draw.text(x, draw.vcenter(), title,
                                 _focused_client_floating
                                     ? _config.floating_title_color
                                     : _config.title_color));
  return x;
}
void DwmBlock::update() { _connection.handle_event(); }
