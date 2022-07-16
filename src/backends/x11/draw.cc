#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

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
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "../../format.hh"
#include "../../log.hh"
#include "../../util.hh"
#include "draw.hh"

XftColor *XDraw::lookup_xft_color(color color) {
  if (auto c = _xft_color_cache.find(color); c != _xft_color_cache.end())
    return &c->second;

  // This color type juggling here is necessary because XftDrawString8
  // requires an XftColor which requires an XRenderColor
  color::rgb rgb = color;
  XRenderColor xrandr_color{.red = static_cast<unsigned short>(rgb.r * 0xff),
                            .green = static_cast<unsigned short>(rgb.g * 0xff),
                            .blue = static_cast<unsigned short>(rgb.b * 0xff),
                            .alpha = 0xffff};
  XftColor xft_color;
  if (XftColorAllocValue(_dpy, _visual, _cmap, &xrandr_color, &xft_color) != 1)
    throw std::runtime_error("XftColorAllocValue failed");

  return &_xft_color_cache.emplace(color, std::move(xft_color)).first->second;
}

XftFont *XDraw::lookup_font(char32_t codepoint) {
  if (auto f = _font_cache.find(codepoint); f != _font_cache.end())
    return f->second;

  for (const auto &font : _fonts) {
    if (XftCharExists(_dpy, font, codepoint)) {
      _font_cache.emplace(codepoint, font);
      return font;
    }
  }

  if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t' ||
      codepoint == '\v' || codepoint == '\f' || codepoint == '\b' ||
      codepoint == '\0' || codepoint == '\e' || codepoint == '\a')
    return nullptr;

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
  _font_cache.emplace(codepoint, nullptr);
  if (res != std::codecvt_base::ok)
    std::print(warn, "Invalid codepoint 0x{:0>8X}\n", (std::uint32_t)codepoint);
  else {
    out[last_out - out.begin()] = '\0';
    std::string_view sv{out.begin(),
                        (std::string_view::size_type)(last_out - out.begin())};
    // Ignore some common characters not meant to be handled by fonts.
    std::print(warn, "Could not find font for codepoint 0x{:0>8X} ('{}')\n",
               (std::uint32_t)codepoint, sv);
  }
  return nullptr;
}

class codepoint_iterator {
  std::string_view _str;
  std::string_view::const_iterator _it{_str.begin()};

  inline std::size_t utf8_seq_len(char seq_begin) const {
    if ((seq_begin & 0x80) == 0)
      return 1;
    if ((seq_begin & 0xE0) == 0xC0)
      return 2;
    if ((seq_begin & 0xF0) == 0xE0)
      return 3;
    if ((seq_begin & 0xF8) == 0xF0)
      return 4;
    throw std::runtime_error("Invalid UTF-8 sequence");
  }
  inline bool utf8_is_first_char(char seq_begin) const {
    if ((seq_begin & 0x80) == 0)
      return true;
    if ((seq_begin & 0xC0) == 0x80)
      return false;
    throw std::runtime_error("Invalid UTF-8 sequence");
  }

public:
  using value_type = char32_t;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type *;
  using reference = value_type &;
  using iterator_category = std::random_access_iterator_tag;

  class sentinel {};

  explicit codepoint_iterator(std::string_view str) : _str(str) {}

  std::string_view::const_iterator base() const { return _it; }

  codepoint_iterator &operator++() {
    _it += utf8_seq_len(*_it);
    if (_it > _str.end())
      _it = _str.end();
    return *this;
  }

  codepoint_iterator operator++(int) {
    auto ret = *this;
    ++(*this);
    return ret;
  }

  codepoint_iterator &operator--() {
    while (!utf8_is_first_char(*_it)) {
      --_it;
      if (_it == _str.begin())
        break;
    }
    return *this;
  }

  codepoint_iterator operator--(int) {
    auto ret = *this;
    --(*this);
    return ret;
  }

  codepoint_iterator &operator+=(std::ptrdiff_t n) {
    while (n > 0) {
      _it += utf8_seq_len(*_it);
      if (_it > _str.end()) {
        _it = _str.end();
        break;
      }
      --n;
    }
    return *this;
  }

  codepoint_iterator &operator-=(std::ptrdiff_t n) {
    while (n > 0)
      --(*this), --n;
    return *this;
  }

  codepoint_iterator operator+(std::ptrdiff_t n) const {
    auto ret = *this;
    ret += n;
    return ret;
  }

  codepoint_iterator operator-(std::ptrdiff_t n) const {
    auto ret = *this;
    ret -= n;
    return ret;
  }

  bool operator==(const sentinel &) const { return _it == _str.end(); }
  bool operator==(std::default_sentinel_t) const { return _it == _str.end(); }

  auto operator<=>(const codepoint_iterator &other) const {
    return _it <=> other._it;
  }

  char32_t operator*() const {
    if (_it == _str.end())
      return 0;

    static auto &cvt =
        std::use_facet<std::codecvt<char32_t, char, std::mbstate_t>>(
            std::locale());
    static std::mbstate_t state{};

    const char *last_in;
    char32_t codepoint;
    char32_t *last_out;

    std::codecvt_base::result res =
        cvt.in(state, &*_it, _str.data() + _str.size(), last_in, &codepoint,
               &codepoint + 1, last_out);

    if (res != std::codecvt_base::result::ok &&
        res != std::codecvt_base::result::partial)
      throw std::runtime_error("Invalid UTF-8 string");

    assert(last_in == &*_it + utf8_seq_len(*_it));

    return codepoint;
  }
};

Draw::pos_t XDraw::text(pos_t x, pos_t y, std::string_view text, color color) {
  XSetForeground(_dpy, _gc, color.as_rgb());

  // FIXME: Why?
  --y;

  auto xft_color = lookup_xft_color(color);
  size_t width = 0;
  XftFont *current_font = nullptr;
  std::string_view::const_iterator current_begin = text.begin();

  for (auto it = codepoint_iterator(text); it != codepoint_iterator::sentinel();
       ++it) {
    if (auto font = lookup_font(*it)) {
      if (current_font != nullptr && current_font != font) {
        XGlyphInfo extents;
        static_assert(sizeof(FcChar32) == sizeof(char32_t));
        XftTextExtentsUtf8(_dpy, current_font,
                           reinterpret_cast<const FcChar8 *>(&*current_begin),
                           std::distance(current_begin, it.base()), &extents);
        XftDrawStringUtf8(_xft_draw, xft_color, current_font, x,
                          y + extents.y / 2,
                          reinterpret_cast<const FcChar8 *>(&*current_begin),
                          std::distance(current_begin, it.base()));
        width += extents.xOff;
        x += extents.xOff;

        current_begin = it.base();
      }
      current_font = font;
    }
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

template <typename Iter, typename End>
requires requires(Iter it, End end) {
  { it != end } -> std::convertible_to<bool>;
  { *it } -> std::convertible_to<char32_t>;
  {++it};
}
Draw::pos_t XDraw::_iterator_textw(Iter it, End end) {
  size_t total = 0;

  for (; it != end; ++it) {
    if (auto font = lookup_font(*it)) {
      auto codepoint = *it;
      XGlyphInfo info;
      XftTextExtents32(
          _dpy, font, reinterpret_cast<const FcChar32 *>(&codepoint), 1, &info);
      total += info.xOff;
    }
  }

  return total;
}

Draw::pos_t XDraw::textw(std::string_view text) {
  return _iterator_textw(codepoint_iterator(text), std::default_sentinel);
}

Draw::pos_t XDraw::textw(std::u32string_view text) {
  return _iterator_textw(text.begin(), text.end());
}
