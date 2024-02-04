#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <pango/pango.h>
#include <pango/pangoxft.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fontconfig/fontconfig.h>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>

// TEMPORARY
#include <execinfo.h>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "../../log.hh"
#include "../../util.hh"
#include "draw.hh"

namespace ui::x11 {

draw::pos_t draw::text(pos_t x, pos_t y, std::string_view text, color color) {
  auto *cached = _text_cache.get(text);
  if (cached == NULL)
    cached = &_text_cache.insert(std::string(text), _text_full(_text_prepare(text)));

  // if (std::abs((int)cached->logical_size.x - (int)cached->ink_size.x) > 10)
  //   debug << "differs on " << std::quoted(text) << ": " << cached->logical_size << ' ' << cached->ink_size << '\n';

  if (cached->ink_size.is_zero())
    return cached->logical_size.x;

  XGCValues gcv;
  XGetGCValues(_dpy, _gc, GCFunction, &gcv);
  int previous_function = gcv.function;

  Pixmap texture = cached->texture;
  if (color != 0xFFFFFF) {
    // debug << "recolor " << cached->size.x << ' ' << cached->size.y << '\n';
    auto colored = XCreatePixmap(_dpy, _drw, cached->ink_size.x, cached->ink_size.y, XDefaultDepth(_dpy, 0));
    XSync(_dpy, true);
    XSetForeground(_dpy, _gc, color.as_rgb());
    XFillRectangle(_dpy, colored, _gc, 0, 0, cached->ink_size.x, cached->ink_size.y);

    gcv.function = GXand;
    XChangeGC(_dpy, _gc, GCFunction, &gcv);
    XCopyArea(_dpy, texture, colored, _gc, 0, 0, cached->ink_size.x, cached->ink_size.y, 0, 0);

    texture = colored;
  }

  // if (text.find("GiB") != std::string_view::npos)
  //   debug << "\x1b[1mdrawing\x1b[0m " << std::quoted(text) << " with offset " << cached->offset
  //         << " (is:" << cached->ink_size << ", ls:" << cached->logical_size << ")\n";

  x += cached->offset.x;
  y += cached->offset.y;

  gcv.function = GXandInverted;
  XChangeGC(_dpy, _gc, GCFunction, &gcv);
  XCopyArea(_dpy, texture, _drw, _gc, 0, 0, cached->ink_size.x, cached->ink_size.y, x, y);

  gcv.function = GXor;
  XChangeGC(_dpy, _gc, GCFunction, &gcv);
  XCopyArea(_dpy, texture, _drw, _gc, 0, 0, cached->ink_size.x, cached->ink_size.y, x, y);

  gcv.function = previous_function;
  XChangeGC(_dpy, _gc, GCFunction, &gcv);

  if (texture != cached->texture)
    XFreePixmap(_dpy, texture);

  return cached->logical_size.x;
}

std::array<char, 5> codepoint_to_string(char32_t codepoint) {
  const auto &cvt = std::use_facet<std::codecvt<char32_t, char, std::mbstate_t>>(std::locale());
  std::mbstate_t state{};

  const char32_t *last_in;
  char *last_out;
  std::array<char, 5> out{};
  std::codecvt_base::result res =
      cvt.out(state, (char32_t *)&codepoint, (char32_t *)&codepoint + 1, last_in, out.begin(), out.end() - 1, last_out);
  if (res != std::codecvt_base::ok)
    throw std::runtime_error("codepoint_to_string: invalid codepoint");
  out[last_out - out.begin()] = '\0';
  return out;
}

draw::PreparedText draw::_text_prepare(std::string_view text) {
  PangoAttrIterator *iterator = pango_attr_list_get_iterator(_pango_itemize_attrs);
  GList *items = pango_itemize(_fonts->_pango, text.data(), 0, text.size(), _pango_itemize_attrs, iterator);
  pango_attr_iterator_destroy(iterator);

  std::vector<PangoGlyphString *> item_glyphs;
  for (GList *it = items; it; it = it->next) {
    PangoItem *item = (PangoItem *)it->data;
    PangoGlyphString *glyphs = pango_glyph_string_new();
    // debug << "shaping item " << text.substr(item->offset, item->length) << '\n';
    pango_shape_full(text.data() + item->offset, item->length, text.data(), text.size(), &item->analysis, glyphs);
    item_glyphs.push_back(glyphs);
  }

#define PPROP(r, p) #p ": " << r.p
#define PRECT(r) PPROP(r, x) << ' ' << PPROP(r, y) << ' ' << PPROP(r, width) << ' ' << PPROP(r, height)
  std::vector<ivec2> item_offsets;
  PangoRectangle ink{};
  PangoRectangle logical{};

  int i = 0;
  for (auto *it = items; it; it = it->next, ++i) {
    auto *item = (PangoItem *)it->data;
    PangoRectangle current_ink;
    PangoRectangle current_logical;

    PangoGlyphString *glyphs = item_glyphs[i];
    // debug << "glyph count: " << glyphs->num_glyphs << '\n';
    // debug << "glyphs:";
    // for (auto glyph : std::span(glyphs->glyphs, glyphs->num_glyphs))
    //   debug << ' ' << glyph.glyph << " (" << glyph.geometry.width << ")";
    // debug << '\n';
    // debug << "getting extents for item of length " << item->num_chars << "(" << item->length << ") at " <<
    // item->offset
    //       << '\n';
    // debug << "analysis chose font: "
    //       << pango_font_family_get_name(pango_font_face_get_family(pango_font_get_face(item->analysis.font))) <<
    //       '\n';

    pango_glyph_string_extents(glyphs, item->analysis.font, &current_ink, &current_logical);
    // debug << "pango logical: " << PRECT(current_logical) << '\n';
    // debug << PRECT(ink) << " + " << PRECT(current_ink) << '\n';

    item_offsets.push_back({logical.width, current_ink.y});
    // debug << "pango offset: " << item_offsets.back() << '\n';

    logical.height = std::max(logical.height, current_logical.height);
    logical.width += current_logical.width + current_logical.x;
    logical.y = std::min(current_logical.y, logical.y);

    if (it == items)
      ink.width += current_ink.width;
    else if (it->next == NULL)
      ink.width += current_ink.width + current_ink.x;
    else
      ink.width += current_logical.width;
    ink.height = std::max(ink.height, current_ink.height);
    ink.y = std::min(current_ink.y, ink.y);
    if (it == items)
      ink.x = current_ink.x;

    // debug << "= " << PRECT(ink) << '\n';
  }

  pango_extents_to_pixels(&ink, &logical);

  if (ink.width > 10000 || ink.height > 10000)
    g_error("THE EXTENTS ARE FUCKED UP AGAIN");

  // debug << "pixels ink " << PRECT(ink) << '\n';
  // debug << "pixels logical " << PRECT(logical) << '\n';

  return PreparedText(std::string(text), std::move(item_glyphs), items, std::move(item_offsets), ink, logical);
}

void draw::_text_at(XftDraw *draw, PreparedText const &text, pos_t x, pos_t y, color color) {
  color::rgb rgb = color.as_rgb();

  PangoRenderer *renderer = pango_xft_renderer_new(_dpy, XDefaultScreen(_dpy));
  PangoColor pango_color;
  char color_spec[8];
  snprintf(color_spec, 8, "#%02X%02X%02X", rgb.r, rgb.g, rgb.b);
  // debug << "color: " << color_spec << '\n';
  g_assert(pango_color_parse(&pango_color, color_spec));
  pango_xft_renderer_set_default_color((PangoXftRenderer *)renderer, &pango_color);
  pango_xft_renderer_set_draw((PangoXftRenderer *)renderer, draw);

  int i = 0;
  for (auto *it = text.items; it; it = it->next, ++i) {
    auto *item = (PangoItem *)it->data;

    PangoGlyphItem glyph_item{
        .item = item,
        .glyphs = text.item_glyphs[i],
        .y_offset = 0,
        .start_x_offset = 0,
        .end_x_offset = 0,
    };
    // debug << "drawing " << std::quoted(text.original_text.substr(item->offset, item->length)) << " at x offset "
    //       << (int)x + text.item_offsets[i].x / PANGO_SCALE << '\n';
    pango_renderer_draw_glyph_item(renderer, text.original_text.data(), &glyph_item,
                                   (int)x * PANGO_SCALE + text.item_offsets[i].x, y * PANGO_SCALE);
  }
}

draw::CachedText draw::_text_full(draw::PreparedText const &prep) {
  if (prep.ink_extents.width > 0 && prep.ink_extents.height > 0) {
    Pixmap pixmap = XCreatePixmap(_dpy, _drw, prep.ink_extents.width, prep.ink_extents.height, XDefaultDepth(_dpy, 0));
    XSync(_dpy, true);
    XSetForeground(_dpy, _gc, 0x000000);
    XFillRectangle(_dpy, pixmap, _gc, 0, 0, prep.ink_extents.width, prep.ink_extents.height);
    XSetForeground(_dpy, _gc, 0xFFFFFF);
    auto xft_draw = XftDrawCreate(_dpy, pixmap, DefaultVisual(_dpy, 0), DefaultColormap(_dpy, 0));

    _text_at(xft_draw, prep, -prep.ink_extents.x, -prep.ink_extents.y, 0xFFFFFF);

    XftDrawDestroy(xft_draw);

    auto off = -prep.logical_extents.y - prep.logical_extents.height / 2;
    // debug << "[HELP ME] BASELINE WILL BE OFFSET BY " << off << '\n';
    return CachedText(_dpy, pixmap, {prep.ink_offset().x, prep.ink_offset().y + off}, prep.ink_size(),
                      prep.logical_size());
  } else
    return CachedText(uvec2{0, 0}, prep.logical_size());
}

uvec2 draw::textsz(std::string_view text) {
  auto *cached = _text_cache.get(text);
  if (cached == NULL)
    cached = &_text_cache.insert(std::string(text), _text_full(_text_prepare(text)));

  return cached->logical_size;
}

} // namespace ui::x11
