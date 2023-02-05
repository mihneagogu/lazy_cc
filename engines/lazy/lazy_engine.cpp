#include "lazy_engine.h"

namespace lazy {

  Clock Globals::clock = Clock();
  int Clock::time() const { return current_time_; }

} // namespace lazy

