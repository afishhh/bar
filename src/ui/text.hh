#pragma once

#include <ranges>

#include "../util.hh"
#include "fonts.hh"
#include "gl.hh"
#include "ui.hh"
#include "util.hh"

namespace ui {

class TextRenderer {
  struct CachedText {
    unsigned texture;
    ivec2 offset;
    uvec2 ink_size;
    uvec2 logical_size;

    CachedText(uvec2 ink_size, uvec2 logical_size) : texture(0), ink_size(ink_size), logical_size(logical_size) {}
    CachedText(unsigned texture, ivec2 offset, uvec2 ink_size, uvec2 logical_size)
        : texture(texture), offset(offset), ink_size(ink_size), logical_size(logical_size) {}
    CachedText(CachedText const &) = delete;
    CachedText(CachedText &&other)
        : texture(other.texture), offset(other.offset), ink_size(other.ink_size), logical_size(other.logical_size) {
      other.texture = 0;
    }

    CachedText &operator=(CachedText const &) = delete;
    CachedText &operator=(CachedText &&other) {
      texture = other.texture;
      ink_size = other.ink_size;
      logical_size = other.logical_size;
      other.texture = 0;
      return *this;
    }

    ~CachedText() {
      if (texture != 0) {
        glDeleteTextures(1, &texture);
        fmt::println(debug, "Texture {} destroyed", texture);
      }
    }
  };

  // TODO: A time-based cache instead?
  //       Or a "cycle"-based one instead.
  //       Maybe if a certain texture is not reused after a few redraws it gets deleted.
  LRUMap<std::string, CachedText, 512> _text_cache;

  PangoAttrList *_pango_itemize_attrs;
  std::shared_ptr<fonts> _fonts;
  uvec2 _size;

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
  unsigned _create_texture(PreparedText const &string);
  CachedText _text_full(PreparedText const &text);

public:
  TextRenderer() : _pango_itemize_attrs(nullptr), _fonts(nullptr) {}
  BAR_NON_COPYABLE(TextRenderer);
  BAR_NON_MOVEABLE(TextRenderer);
  ~TextRenderer() { pango_attr_list_unref(_pango_itemize_attrs); }

  void set_fonts(std::shared_ptr<fonts> &&fonts) {
    _fonts = std::move(fonts);

    if (_pango_itemize_attrs)
      pango_attr_list_unref(_pango_itemize_attrs);
    _pango_itemize_attrs = pango_attr_list_new();
    for (auto d : _fonts->_descriptions | std::views::reverse)
      pango_attr_list_insert(_pango_itemize_attrs, pango_attr_font_desc_new(d));
  }
  std::shared_ptr<class fonts> const &get_fonts() { return _fonts; }

  struct Result {
    uvec2 logical_size;
    uvec2 ink_size;
    ivec2 draw_offset;
    unsigned texture;
  };

  Result render(std::string_view text);
  uvec2 size(std::string_view text);
};

} // namespace ui
