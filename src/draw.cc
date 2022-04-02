#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <iostream>

#include "draw.hh"

// Stolen from dwm/drw.c
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

size_t Draw::text(size_t x, size_t y, std::string_view text, color_type color) {
  check_coords(x, y);
  XSetForeground(_dpy, _gc, color);

  if (!_color_cache.contains(color)) {
    // This color type juggling here is necessary because XftDrawString8
    // requires an XftColor which requires an XRenderColor
    XftColor xft_color;
    auto xrandr_color =
        XRenderColor{.red = (unsigned short)((color >> 16 & 0xFF) * 257),
                     .green = (unsigned short)((color >> 8 & 0xFF) * 257),
                     .blue = (unsigned short)((color & 0xFF) * 257),
                     .alpha = 0xffff};
    if (XftColorAllocValue(_dpy, _visual, _cmap, &xrandr_color, &xft_color) !=
        1)
      throw std::runtime_error("XftColorAllocValue failed");

    _color_cache[color] = std::move(xft_color);
  }

  auto total_width = 0;

  for (auto it = text.begin(); it != text.end();) {
    long utf8;
    size_t len = utf8decode(it, &utf8, UTF_SIZ);
    bool found = false;

    for (auto font : _fonts) {
      if (XftCharExists(_dpy, font, utf8)) {
        XftDrawStringUtf8(_xft_draw, &_color_cache[color], font, x + _offset_x,
                          y + _offset_y + (font->descent + font->ascent) / 4,
                          (FcChar8 *)&*it, len);
        XGlyphInfo info;
        XftTextExtentsUtf8(_dpy, font, (FcChar8 *)&*it, len, &info);
        x += info.xOff;
        total_width += info.xOff;
        found = true;
        break;
      }
    }
    if (!found) {
      std::cerr << "Could not find char '" << (char)utf8 << "' (0x" << std::hex
                << utf8 << ") in any font\n";
    }

    it += len;
  }

  return total_width;
}

size_t Draw::text_width(std::string_view text) const {
  size_t total = 0;
  // for (auto it = text.begin(); it != text.end();) {
  //   long utf8;
  //   auto len = utf8decode(it, &utf8, UTF_SIZ);
  //   bool found = false;
  //   for (auto font : _fonts) {
  //     if (XftCharExists(_dpy, font, utf8)) {
  //       XGlyphInfo info;
  //       XftTextExtentsUtf8(_dpy, font, (FcChar8 *)&utf8, 1, &info);
  //       total += info.xOff;
  //       found = true;
  //       break;
  //     }
  //   }
  //   if (!found) {
  //     std::cerr << "Could not find char " << (char)utf8 << " (" << std::hex
  //               << utf8 << ") in any font\n";
  //   }
  //   it += len;
  // }
  return total;
}
