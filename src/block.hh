#pragma once

#include <chrono>
#include <cstddef>

#include "draw.hh"
#include "loop.hh"

class Block {
public:
  virtual ~Block() {}

  // FIXME: Workaroud for "Static Initiaisation Order Fiasco".
  //        dwmipcpp has a static hashmap which is initialised after the blocks
  //        are and when DwmBlock tries to intialise a Connection with dwm that
  //        hashmap is accessed and causes a SIGFPE floating point exception.
  virtual void late_init(){};

  virtual size_t draw(Draw &, std::chrono::duration<double> delta) = 0;

  virtual void animate(EventLoop::duration delta){};
  virtual std::optional<EventLoop::duration> animate_interval() {
    return std::nullopt;
  }
  virtual void update(){};
  virtual std::chrono::duration<double> update_interval() {
    return std::chrono::duration<double>::max();
  };
};
