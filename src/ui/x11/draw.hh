#pragma once

#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <pango/pango-glyph.h>
#include <pango/pango-types.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../draw.hh"
#include "../util.hh"
#include "fonts.hh"
#include "pango/pango-attributes.h"

namespace ui::x11 {

class draw final : public ui::draw {
private:
  friend class window;

  struct CachedText {
    // annoyingly increases the size of this structure
    Display *dpy;

    Pixmap texture;
    ivec2 offset;
    uvec2 ink_size;
    uvec2 logical_size;

    CachedText(uvec2 ink_size, uvec2 logical_size)
        : dpy(nullptr), texture(0), ink_size(ink_size), logical_size(logical_size) {}
    CachedText(Display *dpy, Pixmap tex, ivec2 offset, uvec2 ink_size, uvec2 logical_size)
        : dpy(dpy), texture(tex), offset(offset), ink_size(ink_size), logical_size(logical_size) {}
    CachedText(CachedText const &) = delete;
    CachedText(CachedText &&other)
        : dpy(other.dpy), texture(other.texture), offset(other.offset), ink_size(other.ink_size),
          logical_size(other.logical_size) {
      other.dpy = nullptr;
    }
    CachedText &operator=(CachedText const &) = delete;
    CachedText &operator=(CachedText &&other) {
      dpy = other.dpy;
      texture = other.texture;
      ink_size = other.ink_size;
      logical_size = other.logical_size;
      other.dpy = nullptr;
      return *this;
    }

    ~CachedText() {
      if (dpy) {
        // debug << "dealloc pixmap " << texture << '\n';
        XFreePixmap(dpy, texture);
      }
    }
  };
  // TODO: See if a separate "long term" and "short term" cache would make sense
  //       std::unordered_map<size_t, size_t> _text_hash_frequency;
  LRUMap<std::string, CachedText, 1024> _text_cache;

  Display *_dpy;
  Drawable _win;
  Drawable _drw;
  GC _gc;
  std::shared_ptr<fonts> _fonts;
  XftDraw *_xft_draw;
  uvec2 _size;

  Visual *_visual = DefaultVisual(_dpy, DefaultScreen(_dpy));
  Colormap _cmap = XCreateColormap(_dpy, _win, _visual, AllocNone);

  PangoAttrList *_pango_itemize_attrs;

  struct PreparedText {
    std::string original_text;
    std::vector<PangoGlyphString *> item_glyphs;
    GList *items;
    std::vector<ivec2> item_offsets;
    PangoRectangle ink_extents;
    PangoRectangle logical_extents;

    ivec2 ink_offset() const { return {ink_extents.x, ink_extents.y}; }
    uvec2 ink_size() const { return {(unsigned)ink_extents.width, (unsigned)ink_extents.height}; }
    uvec2 logical_size() const { return {(unsigned)logical_extents.width, (unsigned)logical_extents.height}; }

    PreparedText(std::string &&original_text, std::vector<PangoGlyphString *> &&item_strings, GList *items,
                 std::vector<ivec2> &&item_offsets, PangoRectangle ink_extents, PangoRectangle logical_extents)
        : original_text(std::move(original_text)), item_glyphs(std::move(item_strings)), items(items),
          item_offsets(std::move(item_offsets)), ink_extents(ink_extents), logical_extents(logical_extents) {}
    PreparedText(PreparedText const &) = delete;
    PreparedText &operator=(PreparedText const &) = delete;
    PreparedText(PreparedText &&other)
        : original_text(std::move(other.original_text)), item_glyphs(std::move(other.item_glyphs)), items(other.items),
          item_offsets(std::move(other.item_offsets)), ink_extents(other.ink_extents),
          logical_extents(other.logical_extents) {
      other.items = NULL;
    }
    PreparedText &operator=(PreparedText &&other) {
      original_text = std::move(other.original_text);
      item_glyphs = std::move(other.item_glyphs);
      items = other.items;
      item_offsets = std::move(other.item_offsets);
      ink_extents = other.ink_extents;
      logical_extents = other.logical_extents;
      other.items = NULL;
      return *this;
    }
    ~PreparedText() {
      if (items)
        g_list_free_full(items, (void (*)(void *))pango_item_free);
      for (auto glyphs : item_glyphs)
        pango_glyph_string_free(glyphs);
    }
  };

  PreparedText _text_prepare(std::string_view text);
  void _text_at(XftDraw *xft_draw, PreparedText const &string, pos_t x, pos_t y, color color);
  CachedText _text_full(PreparedText const &text);

public:
  draw(x11::connection *conn, Window window, Drawable drawable, uvec2 size)
      : _dpy(conn->display()), _win(window), _drw(drawable), _size(size), _pango_itemize_attrs(nullptr) {
    _gc = XCreateGC(conn->display(), drawable, 0, nullptr);
    _xft_draw = XftDrawCreate(conn->display(), drawable, DefaultVisual(conn->display(), 0),
                              DefaultColormap(conn->display(), 0));
    set_fonts(std::make_shared<fonts>(conn));
  }

  ~draw() {
    XftDrawDestroy(_xft_draw);
    XFreeGC(_dpy, _gc);
    XFreeColormap(_dpy, _cmap);
    pango_attr_list_unref(_pango_itemize_attrs);
  }

  void set_fonts(std::shared_ptr<fonts> fonts) {
    _fonts = std::move(fonts);

    if (_pango_itemize_attrs)
      pango_attr_list_unref(_pango_itemize_attrs);
    _pango_itemize_attrs = pango_attr_list_new();
    for (auto d : _fonts->_descriptions | std::views::reverse)
      pango_attr_list_insert(_pango_itemize_attrs, pango_attr_font_desc_new(d));
  }
  std::shared_ptr<class fonts> const &get_fonts() { return _fonts; }

  pos_t width() const override { return _size.x; }
  pos_t height() const override { return _size.y; }

  pos_t vcenter() const override { return height() / 2; }
  pos_t hcenter() const override { return width() / 2; }

  void fcircle(pos_t x, pos_t y, pos_t d, color color) override {
    XSetForeground(_dpy, _gc, color.as_rgb());
    XFillArc(_dpy, _drw, _gc, x, y, d, d, 0, 23040);
  }

  void hrect(pos_t x, pos_t y, pos_t w, pos_t h, color color) override {
    XSetForeground(_dpy, _gc, color.as_rgb());
    XDrawRectangle(_dpy, _drw, _gc, x, y, w, h);
  }
  void frect(pos_t x, pos_t y, pos_t width, pos_t height, color color) override {
    XSetForeground(_dpy, _gc, color.as_rgb());
    XFillRectangle(_dpy, _drw, _gc, x, y, width, height);
  }
  void rect(pos_t x, pos_t y, pos_t width, pos_t height, color color) { frect(x, y, width, height, color); }
  void line(pos_t x1, pos_t y1, pos_t x2, pos_t y2, color color) override {
    XSetForeground(_dpy, _gc, color.as_rgb());
    XDrawLine(_dpy, _drw, _gc, x1, y1, x2, y2);
  }

  pos_t text(pos_t x, pos_t y, std::string_view, color color) override;

  uvec2 textsz(std::string_view text) final override;
};

} // namespace ui::x11
