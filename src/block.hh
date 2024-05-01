#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <uv.h>

#include "bufdraw.hh"
#include "log.hh"
#include "ui/draw.hh"

class Block {
public:
  Block() = default;
  virtual ~Block() = default;

  Block(Block const &) = delete;
  Block(Block &&) = delete;
  Block &operator=(Block const &) = delete;
  Block &operator=(Block &&) = delete;

  using Interval = std::chrono::steady_clock::duration;

  virtual void setup() {}
  virtual bool skip() { return false; }
  virtual void delay_draw() {}
  virtual void animate(Interval) {}

  virtual size_t draw(ui::draw &, std::chrono::duration<double>, size_t, bool) = 0;

  virtual bool has_tooltip() const { return false; }
  virtual void draw_tooltip(ui::draw &, std::chrono::duration<double>, unsigned) const {
    throw std::logic_error("Block::draw_tooltip called but not implemented");
  };
};

class SimpleBlock : public Block {
  uv_timer_t _update_timer;

public:
  SimpleBlock() = default;
  virtual ~SimpleBlock() = default;

  // Workaroud for "Static Initiaisation Order Fiasco".
  // dwmipcpp has a static hashmap which is initialised after the blocks
  // are and when DwmBlock tries to intialise a Connection with dwm that
  // hashmap is accessed and causes a SIGFPE floating point exception.
  virtual void late_init() {}

  virtual size_t draw(ui::draw &, std::chrono::duration<double> delta) = 0;

  size_t draw(ui::draw &draw, std::chrono::duration<double> delta, size_t, bool right) override {
    if (right)
      return this->draw(draw, delta);
    else
      return this->draw(draw, delta);
  }

  virtual void update(){};
  virtual Interval update_interval() { return Interval::max(); };

  void setup() final override {
    late_init();
    update();

    auto loop = uv_default_loop();
    uv_timer_init(loop, &_update_timer);
    _update_timer.data = this;
    auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(update_interval()).count();
    uv_timer_start(
        &_update_timer, [](uv_timer_t *timer) { ((SimpleBlock *)timer->data)->update(); }, interval, interval);
    uv_unref((uv_handle_t *)&_update_timer);
  }

  void delay_draw() override{};
};
