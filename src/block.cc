#include "block.hh"
#include "bar.hh"

#include <chrono>
#include <queue>

void SimpleBlock::setup() {
  late_init();
  update();

  auto loop = uv_default_loop();
  uv_timer_init(loop, &_update_timer);
  _update_timer.data = this;
  auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(update_interval()).count();
  uv_timer_start(
      &_update_timer,
      [](uv_timer_t *timer) {
        ((SimpleBlock *)timer->data)->update();
        bar::instance().schedule_redraw();
      },
      interval, interval);
  uv_unref((uv_handle_t *)&_update_timer);
}
