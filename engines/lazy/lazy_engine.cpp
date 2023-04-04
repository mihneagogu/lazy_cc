#include "lazy_engine.h"
#include "dependency.h"
#include "table.h"
#include "linked_table.h"

namespace lazy {

  Clock Globals::clock_ = Clock();
  LinkedTable* Globals::table_ = nullptr; // initialized later
  DependencyGraph Globals::dep_ = DependencyGraph(Globals::n_slots);

  Time Clock::time() const { return current_time_.load(std::memory_order_seq_cst); }
  Time Clock::advance() { return current_time_.fetch_add(1, std::memory_order_seq_cst); }

  void Globals::shutdown() {
    if (Globals::table_) {
      delete Globals::table_;
    }
  }

} // namespace lazy

