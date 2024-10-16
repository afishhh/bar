#include "bufdraw.hh"
#include "ui/util.hh"

#include <string>

ui::draw::pos_t BufDraw::height() const { return _draw.height(); }
ui::draw::pos_t BufDraw::width() const { return _draw.width(); }

ui::draw::pos_t BufDraw::vcenter() const { return _draw.vcenter(); }
ui::draw::pos_t BufDraw::hcenter() const { return _draw.hcenter(); }

void BufDraw::line(pos_t x1, pos_t y1, pos_t x2, pos_t y2, color c) {
  _buf.push_back(Line{x1, y1, x2, y2, c});
}

void BufDraw::hrect(pos_t x, pos_t y, pos_t w, pos_t h, color c) {
  _buf.push_back(Rect{x, y, w, h, c});
}
void BufDraw::frect(pos_t x, pos_t y, pos_t w, pos_t h, color c) {
  _buf.push_back(FilledRect{x, y, w, h, c});
}

void BufDraw::fcircle(pos_t x, pos_t y, pos_t d, color c) {
  _buf.push_back(FilledCircle{x, y, d, c});
}

ui::draw::pos_t BufDraw::text(pos_t x, pos_t y, std::string_view text,
                              color c) {
  _buf.push_back(Text{x, y, std::string(text), c});
  return _draw.textw(text);
}
uvec2 BufDraw::textsz(std::string_view text) {
  return _draw.textsz(text);
}
