#include <mutex>
#include "dependency.h"

namespace lazy {

bool LastWrite::was_written() const {
  return tx_ != LastWrite::NO_TX;
}

void DependencyGraph::add_dep(Tid tx, Tid on) {
  dependencies_[tx].push_back(txs_[on]);
}

void DependencyGraph::check_dependencies(Tid tx, const std::vector<int> &read_set) {
  // A transaction T1 depends on another, T2, if
  // T1 reads slot "x" and T2 is the last transactied to 
  // have written a sticky to the given slot "x".
  std::shared_lock<std::shared_mutex> read(global_lock_ /* lock now*/);
  std::defer_lock_t defer;
  std::unique_lock<std::shared_mutex> write(global_lock_, defer);
  bool has_dep = false;
  for (int slot : read_set) {
    const auto& prev = last_writes_[slot];
    if (prev.tx_ != LastWrite::NO_TX) {
      if (!has_dep) {
        // SUG: use upgradable locks from boost?
        read.unlock();
        write.lock();
      }
      has_dep = true;
      add_dep(tx, prev.tx_);
    }
  }
}

void DependencyGraph::sticky_written(Tid tx, int slot) {
  last_writes_[slot].tx_ = tx;
  last_writes_[slot].depth_++;
}

} // namespace lazy
