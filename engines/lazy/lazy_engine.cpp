#include "lazy_engine.h"
#include "dependency.h"
#include "table.h"

namespace lazy {

  Clock Globals::clock_ = Clock();
  Table* Globals::table_ = new Table();
  DependencyGraph Globals::dep_ = DependencyGraph();

  Time Clock::time() const { return current_time_; }
  Time Clock::advance() { return ++current_time_; }

  void Globals::shutdown() {
    delete Globals::table_;
  }

} // namespace lazy

