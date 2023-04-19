#include "lazy_engine.h"
#include "dependency.h"
#include "entry.h"
#include "linked_table.h"
#include "tx_coordinator.h"

namespace lazy {

  Clock Globals::clock_ = Clock();
  LinkedTable* Globals::table_ = nullptr; // initialized later
  DependencyGraph Globals::dep_ = DependencyGraph(Globals::n_slots);
  TxCoordinator* Globals::coord_ = nullptr; // initialized later


  Time Clock::time() const { return current_time_.load(std::memory_order_seq_cst); }
  Time Clock::advance() { return current_time_.fetch_add(1, std::memory_order_seq_cst) + 1; }

  void Globals::shutdown() {
    if (Globals::table_) {
      delete Globals::table_;
    }
    if (Globals::coord_) {
      delete Globals::coord_;
    }
  }

} // namespace lazy

