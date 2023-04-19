#include <mutex>


#include "dependency.h"
#include "tx_coordinator.h"
#include "request.h"
#include "logs.h"
#include "../utils.h"

namespace lazy {

bool LastWrite::was_written() const {
  return tx_ != LastWrite::NO_TX;
}

Time DependencyGraph::time_of_last_write_to(int slot) {
  Tid writer = last_writes_[slot].tx_;
  if (writer == LastWrite::NO_TX) {
    return constants::T0;
  }
  return Globals::coord_->tx_at(writer).time();
}

std::vector<Tid> DependencyGraph::get_dependencies(Tid of) {
  std::shared_lock<std::shared_mutex> read(global_lock_ /* lock now*/);
  auto deps = dependencies_.find(of);
  if (deps == dependencies_.end()) {
    return {};
  }
  return deps->second;
}

void DependencyGraph::check_dependencies(Tid tx, const std::vector<int> &read_set) {
  constexpr bool demo = true;

  // A transaction T1 depends on another, T2, if
  // T1 reads slot "x" and T2 is the last tx to 
  // have written a sticky to the given slot "x".
  std::shared_lock<std::shared_mutex> read(global_lock_ /* lock now*/);
  std::unique_lock<std::shared_mutex> write(global_lock_, std::defer_lock_t());
  bool has_dep = false;


  auto add_dep_on_read = [this, &tx, &has_dep, &read, &write](int read_slot) {
    // FIXME: This actually introduces faux Read-After-Write tx dependency if the 
    // read is performed in reality on a previous blind write of the same tx
    const auto& prev = last_writes_[read_slot];
    if (prev.tx_ != LastWrite::NO_TX && prev.tx_ != tx) {
      if (!has_dep) {
        // SUG: use upgradable locks from boost?
        // SUG: Store deps in array and insert them all at once
        // to avoid holding the write lock while traversing the whole set
        read.unlock();
        write.lock();
      }
      has_dep = true;
      dependencies_[tx].push_back(Globals::coord_->tx_at(prev.tx_).tx_id());
    }
  };

  // Is it possible for another tx to modify the last write we are stickifying the slot?
  // No, since the last write is the last tx which has the given record in its write set,
  // so it's handled by the stickification thread,w hich means it's never the case 
  // that this can happen while we are here, since this is the same thread
  if (demo) {
    // When a tx tries to read a value, it must read it from the time of the
    // tx which last wrote to it, at the time of stickification.
    // This ensures that the reads which are performed at substantiation time
    // are the correct ones
    // Our hardcoded tx always reads a value and writes to it after. 
    // After reading each value we then need to assert that we are the last writers
    // (in case we read the same value again)
    
    auto& req = Globals::coord_->tx_at(tx);
    
    add_dep_on_read(req.write1_);
    add_dep_on_read(req.write2_);
    add_dep_on_read(req.write3_);
  }

  if (has_dep) {
    write.unlock(); 
  } else {
    read.unlock();
  }
}

void DependencyGraph::sticky_written(Tid tx, int slot) {
  last_writes_[slot].tx_ = tx;
  last_writes_[slot].depth_++;
}

} // namespace lazy
