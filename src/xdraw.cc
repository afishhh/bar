#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string_view>

#include "log.hh"
#include "xdraw.hh"

// Stolen from git.suckless.org/dwm drw.c
#define BETWEEN(X, A, B) ((A) <= (X) && (X) <= (B))
#define UTF_INVALID 0xFFFD
#define UTF_SIZ 4

static const unsigned char utfbyte[UTF_SIZ + 1] = {0x80, 0, 0xC0, 0xE0, 0xF0};
static const unsigned char utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0,
                                                   0xF8};
static const long utfmin[UTF_SIZ + 1] = {0, 0, 0x80, 0x800, 0x10000};
static const long utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF,
                                         0x10FFFF};

long utf8decodebyte(const char c, size_t *i) {
  for (*i = 0; *i < (UTF_SIZ + 1); ++(*i))
    if (((unsigned char)c & utfmask[*i]) == utfbyte[*i])
      return (unsigned char)c & ~utfmask[*i];
  return 0;
}

static size_t utf8validate(long *u, size_t i) {
  if (!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
    *u = UTF_INVALID;
  for (i = 1; *u > utfmax[i]; ++i)
    ;
  return i;
}

size_t utf8decode(const char *c, long *u, size_t clen) {
  size_t i, j, len, type;
  long udecoded;

  *u = UTF_INVALID;
  if (!clen)
    return 0;
  udecoded = utf8decodebyte(c[0], &len);
  if (!BETWEEN(len, 1, UTF_SIZ))
    return 1;
  for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
    udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
    if (type)
      return j;
  }
  if (j < len)
    return 0;
  *u = udecoded;
  utf8validate(u, len);

  return len;
}

XftColor *XDraw::lookup_color(color_type color) {
  if (auto c = _color_cache.find(color); c != _color_cache.end())
    return &c->second;

  // This color type juggling here is necessary because XftDrawString8
  // requires an XftColor which requires an XRenderColor
  XftColor xft_color;
  auto xrandr_color =
      XRenderColor{.red = (unsigned short)((color >> 16 & 0xFF) * 257),
                   .green = (unsigned short)((color >> 8 & 0xFF) * 257),
                   .blue = (unsigned short)((color & 0xFF) * 257),
                   .alpha = 0xffff};
  if (XftColorAllocValue(_dpy, _visual, _cmap, &xrandr_color, &xft_color) != 1)
    throw std::runtime_error("XftColorAllocValue failed");

  return &_color_cache.emplace(color, std::move(xft_color)).first->second;
}

XftFont *XDraw::lookup_font(long codepoint) {
  if (auto f = _font_cache.find(codepoint); f != _font_cache.end())
    return f->second;

  for (const auto &font : _fonts) {
    if (XftCharExists(_dpy, font, codepoint)) {
      _font_cache.emplace(codepoint, font);
      return font;
    }
  }

  // Convert the utf-8 codepoint into a string
  const auto &cvt =
      std::use_facet<std::codecvt<char32_t, char, std::mbstate_t>>(
          std::locale());
  std::mbstate_t state{};

  const char32_t *last_in;
  char *last_out;
  std::array<char, 5> out{};
  std::codecvt_base::result res =
      cvt.out(state, (char32_t *)&codepoint, (char32_t *)&codepoint + 1,
              last_in, out.begin(), out.end() - 1, last_out);
  if (res != std::codecvt_base::ok)
    throw std::runtime_error("codepoint conversion failed");
  out[last_out - out.begin()] = '\0';
  std::string_view sv{out.begin(), (std::string_view::size_type)(last_out - out.begin())};
      std::print(warn, "Could not find font for codepoint 0x{:0>8X} ('{}')\n",
                 codepoint, sv);
  _font_cache.emplace(codepoint, nullptr);
  return nullptr;
}

Draw::pos_t XDraw::text(pos_t x, pos_t y, std::string_view text,
                        color_type color) {
  XSetForeground(_dpy, _gc, color);

  // FIXME: Why?
  --y;
  x += _offset_x;
  y += _offset_y;

  auto xft_color = lookup_color(color);
  size_t width = 0;
  std::string_view::iterator current_begin = text.begin();
  XftFont *current_font = nullptr;

  for (auto it = text.begin(); it < text.end();) {
    long utf8;
    size_t len = utf8decode(it, &utf8, std::distance(it, text.end()));
    if (len == 0) {
      ++it;
      // Stop on invalid unicode
      break;
    }

    if (auto font = lookup_font(utf8)) {
      if (current_font != nullptr && current_font != font) {
        XGlyphInfo extents;
        XftTextExtentsUtf8(_dpy, current_font,
                           reinterpret_cast<const FcChar8 *>(&*current_begin),
                           std::distance(current_begin, it), &extents);
        XftDrawStringUtf8(_xft_draw, xft_color, current_font, x,
                          y + extents.y / 2,
                          reinterpret_cast<const FcChar8 *>(&*current_begin),
                          std::distance(current_begin, it));
        width += extents.xOff;
        x += extents.xOff;

        current_begin = it;
      }
      current_font = font;
    }

    it += len;
  }

  if (current_font != nullptr) {
    XGlyphInfo extents;
    XftTextExtentsUtf8(_dpy, current_font,
                       reinterpret_cast<const FcChar8 *>(&*current_begin),
                       std::distance(current_begin, text.end()), &extents);
    XftDrawStringUtf8(_xft_draw, xft_color, current_font, x, y + extents.y / 2,
                      reinterpret_cast<const FcChar8 *>(&*current_begin),
                      std::distance(current_begin, text.end()));
    width += extents.xOff;
  }

  return width;
}

Draw::pos_t XDraw::textw(std::string_view text) {
  size_t total = 0;
  for (auto it = text.begin(); it < text.end();) {
    long utf8;
    size_t len = utf8decode(it, &utf8, UTF_SIZ);
    if (len == 0) {
      ++it;
      continue;
    }

    if (auto font = lookup_font(utf8)) {
      XGlyphInfo info;
      XftTextExtentsUtf8(_dpy, font, (FcChar8 *)&*it, len, &info);
      total += info.xOff;
    }

    it += len;
  }

  return total;
}
