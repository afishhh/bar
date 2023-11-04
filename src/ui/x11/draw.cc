#include <X11/X.h>
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

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "../../log.hh"
#include "../../util.hh"
#include "draw.hh"

namespace ui::x11 {

XftColor *draw::lookup_xft_color(color color) {
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

XftFont *draw::lookup_font(char32_t codepoint) {
  if (auto f = _font_cache.find(codepoint); f != _font_cache.end())
    return f->second;

  for (const auto &font : _fonts->_fonts) {
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
    fmt::print(warn, "Invalid codepoint 0x{:0>8X}\n", (std::uint32_t)codepoint);
  else {
    out[last_out - out.begin()] = '\0';
    std::string_view sv{out.begin(),
                        (std::string_view::size_type)(last_out - out.begin())};
    // Ignore some common characters not meant to be handled by fonts.
    fmt::print(warn, "Could not find font for codepoint 0x{:0>8X} ('{}')\n",
               (std::uint32_t)codepoint, sv);
  }
  return nullptr;
}

class codepoint_iterator {
public:
  enum class error_handling {
    exception,
    replace,
  };

private:
  std::string_view _str;
  std::string_view::const_iterator _it{_str.begin()};
  error_handling _error_handling = error_handling::replace;

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
    return (seq_begin & 0xC0) != 0x80;
  }

  inline bool _is_at_partial_sequence() const {
    return utf8_seq_len(*_it) > size_t(_str.end() - _it);
  }

public:
  using value_type = char32_t;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type *;
  using reference = value_type &;
  using iterator_category = std::random_access_iterator_tag;

  class sentinel {};

  explicit codepoint_iterator(std::string_view str) : _str(str) {}
  codepoint_iterator(std::string_view str, error_handling eh)
      : _str(str), _error_handling(eh) {}

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
    do  {
      --_it;
      if (_it == _str.begin())
        break;
    } while(!utf8_is_first_char(*_it));
    return *this;
  }

  codepoint_iterator operator--(int) {
    auto ret = *this;
    --(*this);
    return ret;
  }

  codepoint_iterator &operator+=(std::ptrdiff_t n) {
    while (n > 0) {
      if(_is_at_partial_sequence())
        break;
      _it += utf8_seq_len(*_it);
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

  bool operator==(const sentinel &) const { return _it == _str.end() || _is_at_partial_sequence(); }
  bool operator==(std::default_sentinel_t) const { return _it == _str.end() || _is_at_partial_sequence(); }

  auto operator<=>(const codepoint_iterator &other) const {
    return _it <=> other._it;
  }

  char32_t operator*() const {
    if (_it == _str.end())
      return 0;
    if(_is_at_partial_sequence()) {
      if(_error_handling == error_handling::exception)
        throw std::runtime_error("Cut off UTF-8 sequence encountered");
      return 0;
    }

    static auto &cvt =
        std::use_facet<std::codecvt<char32_t, char, std::mbstate_t>>(
            std::locale());
    static std::mbstate_t state{};

    const char *last_in;
    char32_t codepoint;
    char32_t *last_out;

    std::codecvt_base::result res =
        cvt.in(state, &*_it, &*_str.end(), last_in, &codepoint, &codepoint + 1,
               last_out);

    if (res != std::codecvt_base::result::ok && res != std::codecvt_base::result::partial) {
      error << "Invalid UTF-8 value in " << _str << " at byte "  << _it - _str.begin() << '\n';

      if(_error_handling == error_handling::exception)
        throw std::runtime_error("Invalid UTF-8 string");
      else
        return 0xFFFD;
    }

    assert(last_in == &*_it + utf8_seq_len(*_it));

    return codepoint;
  }
};

draw::pos_t draw::text(pos_t x, pos_t y, std::string_view text, color color) {
  XSetForeground(_dpy, _gc, color.as_rgb());

  // Looks better this way but FIXME: why
  --y;

  auto xft_color = lookup_xft_color(color);
  size_t width = 0;
  XftFont *current_font = nullptr;
  std::string_view::const_iterator current_begin = text.begin();

  auto simple_draw_text = [this, xft_color](pos_t x, pos_t y, XftFont *font,
                                            std::string_view text) {
    XGlyphInfo extents;
    static_assert(sizeof(FcChar32) == sizeof(char32_t));
    XftTextExtentsUtf8(_dpy, font,
                       reinterpret_cast<const FcChar8 *>(&*text.begin()),
                       std::distance(text.begin(), text.end()), &extents);
    XftDrawStringUtf8(_xft_draw, xft_color, font, x, y + extents.y / 2,
                      reinterpret_cast<const FcChar8 *>(&*text.begin()),
                      text.size());
    return extents.xOff;
  };

  for (auto it = codepoint_iterator(text); it != codepoint_iterator::sentinel();
       ++it) {
    if (auto font = lookup_font(*it)) {
      if (current_font != nullptr && current_font != font) {
        auto off = simple_draw_text(x, y, current_font,
                                    std::string_view(current_begin, it.base()));
        width += off;
        x += off;

        current_begin = it.base();
      }
      current_font = font;
    }
  }

  if (current_font != nullptr) {
    auto off = simple_draw_text(x, y, current_font,
                                std::string_view(current_begin, text.cend()));
    width += off;
  }

  return width;
}

template <typename Iter, typename End>
  requires requires(Iter it, End end) {
    { it != end } -> std::convertible_to<bool>;
    { *it } -> std::convertible_to<char32_t>;
    { ++it };
  }
uvec2 draw::_iterator_textsz(Iter it, End end) {
  uvec2 result{0, 0};

  for (; it != end; ++it) {
    if (auto font = lookup_font(*it)) {
      auto codepoint = *it;
      XGlyphInfo info;
      XftTextExtents32(
          _dpy, font, reinterpret_cast<const FcChar32 *>(&codepoint), 1, &info);

      result.x += info.xOff,
          result.y = std::max(result.y, (unsigned)info.height);
    }
  }

  return result;
}

uvec2 draw::textsz(std::string_view text) {
  return _iterator_textsz(codepoint_iterator(text), std::default_sentinel);
}

uvec2 draw::textsz(std::u32string_view text) {
  return _iterator_textsz(text.begin(), text.end());
}

} // namespace ui::x11
