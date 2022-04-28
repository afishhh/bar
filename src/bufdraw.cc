#include "bufdraw.hh"
#include "draw.hh"
#include <string>

Draw::pos_t BufDraw::screen_width() const { return _draw.screen_width(); }
Draw::pos_t BufDraw::screen_height() const { return _draw.screen_height(); }

Draw::pos_t BufDraw::height() const { return _draw.height(); }
Draw::pos_t BufDraw::width() const { return _draw.width(); }

Draw::pos_t BufDraw::vcenter() const { return _draw.vcenter(); }
Draw::pos_t BufDraw::hcenter() const { return _draw.hcenter(); }

void BufDraw::line(pos_t x1, pos_t y1, pos_t x2, pos_t y2, color_type c) {
  _buf.push_back(Line{x1, y1, x2, y2, c});
}

void BufDraw::hrect(pos_t x, pos_t y, pos_t w, pos_t h, color_type c) {
  _buf.push_back(Rect{x, y, w, h, c});
}
void BufDraw::frect(pos_t x, pos_t y, pos_t w, pos_t h, color_type c) {
  _buf.push_back(FilledRect{x, y, w, h, c});
}

Draw::pos_t BufDraw::text(pos_t x, pos_t y, std::string_view text, color_type c) {
  _buf.push_back(Text{x, y, std::string(text), c});
  return _draw.textw(text);
}
Draw::pos_t BufDraw::textw(std::string_view text) { return _draw.textw(text); }
