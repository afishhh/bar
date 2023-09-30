#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <stdexcept>

#include "loop.hh"
#include "ui/draw.hh"

class Block {
public:
  Block() = default;
  virtual ~Block() = default;

  Block(Block const &) = delete;
  Block(Block &&) = default;
  Block &operator=(Block const &) = delete;
  Block &operator=(Block &&) = default;

  // FIXME: Workaroud for "Static Initiaisation Order Fiasco".
  //        dwmipcpp has a static hashmap which is initialised after the blocks
  //        are and when DwmBlock tries to intialise a Connection with dwm that
  //        hashmap is accessed and causes a SIGFPE floating point exception.
  virtual void late_init(){};

  virtual size_t draw(ui::draw &, std::chrono::duration<double> delta) = 0;
  // HACK: Should this function exist?
  //       Should it have a better name?
  //       Should it have a different signature?
  virtual size_t ddraw(ui::draw &, std::chrono::duration<double>, size_t,
                       bool) {
    return 0;
  }

  virtual bool skip() { return false; }

  virtual void animate(EventLoop::duration) {}
  virtual std::optional<EventLoop::duration> animate_interval() {
    return std::nullopt;
  }
  virtual void update(){};
  // FIXME: Use EventLoop::duration instead of std::chrono::duration<double>.
  virtual std::chrono::duration<double> update_interval() {
    return std::chrono::duration<double>::max();
  };

  virtual bool has_tooltip() const { return false; }
  virtual void draw_tooltip(ui::draw &, std::chrono::duration<double>,
                            unsigned) const {
    throw std::logic_error("Block::draw_tooltip called but not implemented");
  }
};
