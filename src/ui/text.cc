// https://dthompson.us/posts/font-rendering-in-opengl-with-pango-and-cairo.html

#include "text.hh"

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

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "../log.hh"
#include "../util.hh"
#include "draw.hh"
#include "gl.hh"

namespace ui {

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

TextRenderer::PreparedText TextRenderer::_text_prepare(std::string_view text) {
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

unsigned TextRenderer::_create_texture(PreparedText const &text) {
  unsigned texture;

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  // TODO: what do these do?
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  auto [width, height] = text.ink_size();
  cairo_surface_t *surface;
  unsigned char *surface_data = new unsigned char[4 * width * height];
  std::fill_n(surface_data, 4 * width * height, 0);
  surface = cairo_image_surface_create_for_data(surface_data, CAIRO_FORMAT_ARGB32, width, height, 4 * width);
  cairo_t *context = cairo_create(surface);

  cairo_set_source_rgba(context, 1, 1, 1, 1);

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

    cairo_move_to(context, -text.ink_extents.x + (double)text.item_offsets[i].x / PANGO_SCALE, -text.ink_extents.y);
    pango_cairo_show_glyph_item(context, text.original_text.data(), &glyph_item);
  }

  // fmt::println("{}x{} texture = {}", width, height, texture);
  // if (width < 80) {
  //   auto stride = width * 4;
  //   for (unsigned y = 0; y < height; ++y) {
  //     for (unsigned x = 0; x < width; ++x) {
  //       unsigned char *data = surface_data + (4 * x + (y * stride));
  //       color::rgb rgb = color::rgb(data[1], data[2], data[3]);
  //       // std::cout << color::hsl(rgb).l << ' ';
  //       unsigned uwu = map_range(color::hsl(rgb).l, 0, 256, 0, 16);
  //       if (data[3])
  //         std::cout << std::hex << (int)data[3];
  //       else
  //         std::cout << "  ";
  //     }
  //     std::cout << '\n';
  //   }
  // }

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, surface_data);

  delete[] surface_data;
  cairo_destroy(context);
  cairo_surface_destroy(surface);

  return texture;
}

TextRenderer::CachedText TextRenderer::_text_full(PreparedText const &prep) {
  if (prep.ink_extents.width > 0 && prep.ink_extents.height > 0) {
    auto off = -prep.logical_extents.y - prep.logical_extents.height / 2;
    // debug << "[HELP ME] BASELINE WILL BE OFFSET BY " << off << '\n';
    return CachedText(_create_texture(prep), {prep.ink_offset().x, prep.ink_offset().y + off}, prep.ink_size(),
                      prep.logical_size());
  } else
    return CachedText(uvec2{0, 0}, prep.logical_size());
}

TextRenderer::Result TextRenderer::render(std::string_view text) {
  auto *cached = _text_cache.get(text);
  if (cached == NULL)
    cached = &_text_cache.insert(std::string(text), _text_full(_text_prepare(text)));

  // if (std::abs((int)cached->logical_size.x - (int)cached->ink_size.x) > 10)
  //   debug << "differs on " << std::quoted(text) << ": " << cached->logical_size << ' ' << cached->ink_size << '\n';

  if (cached->ink_size.is_zero())
    return Result{cached->logical_size, cached->ink_size, {}, 0};

  // if (text.find("GiB") != std::string_view::npos)
  //   debug << "\x1b[1mdrawing\x1b[0m " << std::quoted(text) << " with offset " << cached->offset
  //         << " (is:" << cached->ink_size << ", ls:" << cached->logical_size << ")\n";

  return Result{cached->logical_size, cached->ink_size, cached->offset, cached->texture};
}


uvec2 TextRenderer::size(std::string_view text) {
  auto *cached = _text_cache.get(text);
  if (cached == NULL)
    cached = &_text_cache.insert(std::string(text), _text_full(_text_prepare(text)));

  return cached->logical_size;
}

} // namespace ui
