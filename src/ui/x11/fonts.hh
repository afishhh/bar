#pragma once

#include <X11/Xft/Xft.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <vector>

#include <fmt/core.h>
#include <pango/pango.h>

#include "connection.hh"
#include "pango/pango-context.h"
#include "pango/pango-font.h"
#include "pango/pango-fontmap.h"
#include "pango/pangoxft.h"

namespace ui::x11 {

class fonts final {
  friend class draw;

  x11::connection *_conn;
  std::vector<PangoFont *> _fonts;
  std::vector<PangoFontDescription *> _descriptions;
  PangoContext *_pango;

public:
  fonts(x11::connection *conn) : _conn(conn) {
    _pango = pango_context_new();
    pango_context_set_font_map(_pango, pango_xft_get_font_map(conn->display(), conn->screen_id()));
  };
  fonts(fonts const &) = delete;
  fonts(fonts &&) = default;
  fonts &operator=(fonts const &) = delete;
  fonts &operator=(fonts &&) = delete;
  ~fonts() {
    for (auto font : _fonts)
      g_object_unref(font);
    for (auto description : _descriptions)
      pango_font_description_free(description);
    g_object_unref(_pango);
  }

  void add(std::string_view name) {
    PangoFontMap *font_map = pango_context_get_font_map(_pango);
    PangoFontDescription *description = pango_font_description_from_string(name.data());
    debug << "pango description for " << name << ":\n";
    debug << "  family: " << pango_font_description_get_family(description) << '\n';
    debug << "  style: " << pango_font_description_get_style(description) << '\n';
    debug << "  size: " << pango_font_description_get_size(description) << '\n';
    PangoFont *font = pango_font_map_load_font(font_map, _pango, description);
    _descriptions.push_back(description);

    debug << " font for " << name << " is "
          << pango_font_family_get_name(pango_font_face_get_family(pango_font_get_face(font))) << '\n';

    if (font)
      _fonts.push_back(font);
    else
      throw std::runtime_error(fmt::format("Failed to load font {}", name));
  }
};

} // namespace ui::x11
