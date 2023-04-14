#include <mutex>


#include "dependency.h"
#include "request.h"
#include "logs.h"
#include "../utils.h"

namespace lazy {

bool LastWrite::was_written() const {
  return tx_ != LastWrite::NO_TX;
}

void DependencyGraph::add_txs(const std::vector<Request*>& txs) {
  for (auto* req : txs) {
    txs_[req->tx_id()] = req;
  }
}

Time DependencyGraph::time_of_last_write_to(int slot) {
  Tid writer = last_writes_[slot].tx_;
  cout << "last writer tot slot " << slot << " tid : " << writer << endl;
  if (writer == LastWrite::NO_TX) {
    return constants::T0;
  }
  return tx_of(writer)->time();
}

std::vector<Request*> DependencyGraph::get_dependencies(Tid of) {
  std::shared_lock<std::shared_mutex> read(global_lock_ /* lock now*/);
  auto deps = dependencies_.find(of);
  if (deps == dependencies_.end()) {
    return {};
  }
  return deps->second;
}

Request* DependencyGraph::tx_of(Tid tid) {
  return txs_[tid];
}

void DependencyGraph::check_dependencies(Tid tx, const std::vector<int> &read_set) {
  // TODO: Double-write in same tx problem causing sticky with same timestamp
  constexpr bool demo = true;

  // A transaction T1 depends on another, T2, if
  // T1 reads slot "x" and T2 is the last tx to 
  // have written a sticky to the given slot "x".
  std::shared_lock<std::shared_mutex> read(global_lock_ /* lock now*/);
  std::unique_lock<std::shared_mutex> write(global_lock_, std::defer_lock_t());
  bool has_dep = false;


  auto add_dep_on_read = [this, &tx, &has_dep, &read, &write](int read_slot) -> Time {
    const auto& prev = last_writes_[read_slot];
    if (prev.tx_ != LastWrite::NO_TX && prev.tx_ != tx) {
      if (!has_dep) {
        // SUG: use upgradable locks from boost?
        read.unlock();
        write.lock();
      }
      has_dep = true;
      dependencies_[tx].push_back(tx_of(prev.tx_));
    }
    return prev.tx_ == LastWrite::NO_TX ? constants::T0 : tx_of(prev.tx_)->time();
  };

  // TODO: Make sure this is correct when other txs write.
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
    
    auto* req = tx_of(tx);
    
    Time t1 = add_dep_on_read(req->write1_);
    sticky_written(tx, req->write1_);
    Time t2 = add_dep_on_read(req->write2_);
    sticky_written(tx, req->write2_);
    Time t3 = add_dep_on_read(req->write3_);
    sticky_written(tx, req->write3_);
    req->read1_t_ = t1;
    req->read2_t_ = t2;
    req->read3_t_ = t3;
    cout << "tx " << tx << " reads " << req->write1_ << " from write performed at " << t1 << endl;
    cout << "tx " << tx << " reads " << req->write2_ << " from write performed at " << t2 << endl;
    cout << "tx " << tx << " reads " << req->write3_ << " from write performed at " << t3 << endl;
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
