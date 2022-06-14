#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include "draw.hh"
#include "util.hh"

class BufDraw : public Draw {
  struct Line {
    pos_t x1, y1;
    pos_t x2, y2;
    color stroke_color;
  };
  struct Rect {
    pos_t x1, y1, w, h;
    color border_color;
  };
  struct FilledRect {
    pos_t x1, y1, w, h;
    color fill_color;
  };
  struct Text {
    pos_t x, y;
    std::string text;
    color stroke_color;
  };
  using operation = std::variant<Line, Rect, FilledRect, Text>;

  std::vector<operation> _buf;
  // FIXME: Don't store a draw
  Draw &_draw;

public:
  BufDraw(Draw &draw) : _draw(draw) {}
  ~BufDraw() {}

  BufDraw(BufDraw &&) = default;
  BufDraw(const BufDraw &) = default;

  void draw_offset(pos_t off_x, pos_t off_y) {
    for (auto &op : _buf) {
      std::visit(overloaded{
                     [&](Line &line) {
                       _draw.line(line.x1 + off_x, line.y1 + off_y,
                                  line.x2 + off_x, line.y2 + off_y, line.stroke_color);
                     },
                     [&](Rect &rect) {
                       _draw.hrect(rect.x1 + off_x, rect.y1 + off_y, rect.w,
                                   rect.h, rect.border_color);
                     },
                     [&](FilledRect &rect) {
                       _draw.frect(rect.x1 + off_x, rect.y1 + off_y, rect.w,
                                   rect.h, rect.fill_color);
                     },
                     [&](Text &text) {
                       _draw.text(text.x + off_x, text.y + off_y, text.text,
                                  text.stroke_color);
                     },
                 },
                 op);
    }
  }
  void clear() { _buf.clear(); }

  pos_t screen_width() const final override;
  pos_t screen_height() const final override;

  pos_t height() const final override;
  pos_t width() const final override;

  pos_t vcenter() const final override;
  pos_t hcenter() const final override;

  void line(pos_t x1, pos_t y1, pos_t x2, pos_t y2,
            color = color::rgb(0xFFFFFF)) final override;

  void hrect(pos_t x, pos_t y, pos_t w, pos_t h,
             color = color::rgb(0xFFFFFF)) final override;
  void frect(pos_t x, pos_t y, pos_t w, pos_t h,
             color = color::rgb(0xFFFFFF)) final override;

  pos_t text(pos_t x, pos_t y, std::string_view text,
             color = color::rgb(0xFFFFFF)) final override;
  pos_t textw(std::string_view text) final override;
};
