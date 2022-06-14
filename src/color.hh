#pragma once

#include <variant>

#include "util.hh"

struct color {
  struct hsl;
  struct rgb {
    unsigned char r, g, b;

    constexpr rgb() : r(0), g(0), b(0) {}
    constexpr rgb(unsigned char r, unsigned char g, unsigned char b)
        : r(r), g(g), b(b) {}
    constexpr rgb(unsigned long rgb)
        : r((rgb >> 16) & 0xff), g((rgb >> 8) & 0xff), b(rgb & 0xff) {}
    explicit constexpr rgb(hsl);

    constexpr operator unsigned long() const {
      return (r << 16) | (g << 8) | b;
    }

    constexpr bool operator==(const rgb &other) const {
      return r == other.r && g == other.g && b == other.b;
    }
    constexpr bool operator!=(const rgb &other) const {
      return !(*this == other);
    }
  };
  struct hsl {
    double h, s, l;

    constexpr hsl() : h(0), s(0), l(0) {}
    constexpr hsl(double h, double s, double l) : h(h), s(s), l(l) {}
    explicit constexpr hsl(rgb);

    constexpr bool operator==(const hsl &other) const {
      return h == other.h && s == other.s && l == other.l;
    }
    constexpr bool operator!=(const hsl &other) const {
      return !(*this == other);
    }
  };

private:
  std::variant<rgb, hsl> _value;
  std::variant<std::monostate, rgb, hsl> _cache;

public:
  constexpr color() : _value(rgb(0, 0, 0)) {}
  constexpr color(unsigned long rgb) : _value((struct rgb)(rgb)) {}
  constexpr color(rgb c) : _value(c) {}
  constexpr color(hsl c) : _value(c) {}

  constexpr color(color const &) = default;
  constexpr color(color &&) = default;
  constexpr color &operator=(color const &) = default;
  constexpr color &operator=(color &&) = default;

  constexpr bool operator==(color const &other) const {
    return std::visit(overloaded{[&](rgb const &r) { return r == rgb(other); },
                                 [&](hsl const &h) { return h == hsl(other); }},
                      _value);
  }
  constexpr bool operator!=(color const &other) const {
    return !(*this == other);
  }

  constexpr auto value() const { return _value; }

  constexpr operator rgb() const {
    return std::visit(
        overloaded{[](rgb const &v) { return v; },
                   [this](hsl const &v) {
                     return std::visit(
                         overloaded{[](rgb const &cached) { return cached; },
                                    [&v](auto) { return rgb(v); }},
                         _cache);
                   }},
        _value);
  }
  constexpr operator hsl() const {
    return std::visit(
        overloaded{[](hsl const &v) { return v; },
                   [this](rgb const &v) {
                     return std::visit(
                         overloaded{[](hsl const &cached) { return cached; },
                                    [&v](auto) { return hsl(v); }},
                         _cache);
                   }},
        _value);
  }

  constexpr auto as_rgb() const { return (struct rgb)(*this); }
  constexpr auto as_hsl() const { return (struct hsl)(*this); }
};

namespace std {
template <> struct hash<color::rgb> {
  size_t operator()(color::rgb const &c) const {
    return std::hash<unsigned char>()(c.r) ^
           (std::hash<unsigned char>()(c.g) << 1) ^
           (std::hash<unsigned char>()(c.b) << 2);
  }
};
template <> struct hash<color::hsl> {
  size_t operator()(color::hsl const &c) const {
    return std::hash<double>()(c.h) ^ (std::hash<double>()(c.s) << 1) ^
           (std::hash<double>()(c.l) << 2);
  }
};
template <> struct hash<color> {
  size_t operator()(color const &c) const {
    return std::visit(
        overloaded{
            [](color::rgb const &v) { return std::hash<color::rgb>()(v); },
            [](color::hsl const &v) { return std::hash<color::hsl>()(v); }},
        c.value());
  }
};
}; // namespace std

constexpr color::rgb::rgb(hsl h) {
  if (h.s == 0)
    r = g = b = h.l * 255;
  else {
    auto hue_to_rgb = [](double p, double q, double t) -> double {
      if (t < 0)
        t += 1;
      if (t > 1)
        t -= 1;
      if (t < 1 / 6.0)
        return p + (q - p) * 6 * t;
      if (t < 1 / 2.0)
        return q;
      if (t < 2 / 3.0)
        return p + (q - p) * (2 / 3.0 - t) * 6;
      return p;
    };

    double q = h.l < 0.5 ? h.l * (1 + h.s) : h.l + h.s - h.l * h.s;
    double p = 2 * h.l - q;

    r = hue_to_rgb(p, q, h.h + 1. / 3.0) * 255;
    g = hue_to_rgb(p, q, h.h) * 255;
    b = hue_to_rgb(p, q, h.h - 1. / 3.0) * 255;
  }
}

constexpr color::hsl::hsl(rgb r) {
  double max = std::max(r.r, std::max(r.g, r.b));
  double min = std::min(r.r, std::min(r.g, r.b));
  double delta = max - min;

  l = (max + min) / 2;

  if (delta == 0) {
    h = 0;
    s = 0;
  } else {
    s = l < 0.5 ? delta / (max + min) : delta / (2 - max - min);
    double h_ = delta == 0 ? 0 : (max - r.r) / delta;
    if (r.r == max)
      h = r.g - r.b == 0 ? 5 + h_ : 1 - h_;
    else if (r.g == max)
      h = r.b - r.r == 0 ? 1 + h_ : 3 - h_;
    else
      h = r.r - r.g == 0 ? 3 + h_ : 5 - h_;
    h /= 6;
  }
}
