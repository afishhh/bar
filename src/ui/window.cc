#include "window.hh"

namespace ui {

gdraw &gwindow::drawer() {
  if (_drawer)
    return *_drawer;
  else {
    return *(_drawer = new gdraw(_window));
  }
}

gwindow::~gwindow() {
  delete _drawer;
  _drawer = nullptr;
  asm("" ::: "memory");
}

} // namespace ui
