#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <stdexcept>

#include "bufdraw.hh"
#include "cancel.hh"
#include "loop.hh"
#include "ui/draw.hh"

class Block {
public:
  Block() = default;
  virtual ~Block() = default;

  Block(Block const &) = delete;
  Block(Block &&) = delete;
  Block &operator=(Block const &) = delete;
  Block &operator=(Block &&) = delete;

  virtual void setup() {}
  virtual bool skip() { return false; }
  virtual void delay_draw() {}

  virtual size_t draw(ui::draw &, std::chrono::duration<double>, size_t, bool) = 0;

  virtual bool has_tooltip() const { return false; }
  virtual void draw_tooltip(ui::draw &, std::chrono::duration<double>, unsigned) const {
    throw std::logic_error("Block::draw_tooltip called but not implemented");
  };
};

class SimpleBlock : public Block {
  CancellableThread _seconary_thread;

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

  using Interval = std::chrono::steady_clock::duration;

  virtual void animate(Interval) {}
  virtual std::optional<Interval> animate_interval() { return std::nullopt; }
  virtual void update(){};
  virtual Interval update_interval() { return Interval::max(); };

  void seconary_thread_main(CancellationToken &token);

  void setup() final override {
    late_init();

    update();
    _seconary_thread = CancellableThread([this](CancellationToken &token) { seconary_thread_main(token); });
  }

  void delay_draw() override{};
};
