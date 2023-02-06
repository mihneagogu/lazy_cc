#include "lazy_engine.h"

namespace lazy {

  Clock Globals::clock_ = Clock();
  Table Globals::table_ = Table();

  Time Clock::time() const { return current_time_; }
  Time Clock::advance() { return ++current_time_; }

} // namespace lazy

