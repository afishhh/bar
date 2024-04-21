#include "block.hh"
#include "bar.hh"

#include <chrono>
#include <queue>

void SimpleBlock::seconary_thread_main(CancellationToken &token) {
  enum class Action { UPDATE, ANIMATE };
  struct Queued {
    std::chrono::steady_clock::time_point next;
    Action action;

    bool operator<(Queued const &other) const { return this->next > other.next; }
  };

  std::priority_queue<Queued> _queue;

  auto now = std::chrono::steady_clock::now();
  _queue.emplace(now + update_interval(), Action::UPDATE);
  if (animate_interval())
    _queue.emplace(now + *animate_interval(), Action::ANIMATE);

  while (!token.cancelled()) {
    auto [time, action] = _queue.top();
    _queue.pop();

    now = std::chrono::steady_clock::now();
    auto delta = std::chrono::steady_clock::duration::zero();
    if (action == Action::ANIMATE)
      delta = std::chrono::duration_cast<std::chrono::steady_clock::duration>(*animate_interval());
    if (now < time)
      token.wait_until(time);
    else if (action == Action::ANIMATE)
      delta += time - now;

    switch (action) {
    case Action::UPDATE:
      update();
      break;

    case Action::ANIMATE:
      animate(delta);
      break;
    }

    now = std::chrono::steady_clock::now();
    switch (action) {
    case Action::UPDATE:
      _queue.emplace(now + update_interval(), Action::UPDATE);
      break;

    case Action::ANIMATE:
      _queue.emplace(now + *animate_interval(), Action::ANIMATE);
      break;
    }

    bar::instance().schedule_redraw();
  }

  std::cerr << "secondary exited!\n";
}
