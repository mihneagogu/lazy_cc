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
  cout << "last writer tot slot " << slot << " : " << writer << endl;
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
  std::shared_lock<std::shared_mutex> read(global_lock_ /* lock now*/);
  return txs_[tid];
}

void DependencyGraph::add_dep(Tid tx, Tid on) {
  dependencies_[tx].push_back(txs_[on]);
}


void DependencyGraph::check_dependencies(Tid tx, const std::vector<int> &read_set) {
  constexpr bool demo = true;

  // A transaction T1 depends on another, T2, if
  // T1 reads slot "x" and T2 is the last tx to 
  // have written a sticky to the given slot "x".
  std::shared_lock<std::shared_mutex> read(global_lock_ /* lock now*/);
  std::defer_lock_t defer;
  std::unique_lock<std::shared_mutex> write(global_lock_, defer);
  bool has_dep = false;

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
    auto* req = tx_of(tx);
    Time t1 = time_of_last_write_to(req->write1_);
    Time t2 = time_of_last_write_to(req->write2_);
    Time t3 = time_of_last_write_to(req->write3_);
    req->read1_t_ = t1;
    req->read2_t_ = t2;
    req->read3_t_ = t3;
    cout << "tx " << tx << " reads " << req->write1_ << " from write performed at " << t1 << endl;
    cout << "tx " << tx << " reads " << req->write2_ << " from write performed at " << t2 << endl;
    cout << "tx " << tx << " reads " << req->write3_ << " from write performed at " << t3 << endl;
  }

  // TODO: Get all the deps with a read lock
  // then only at the end get a write lock and write to the dep graph
  for (int slot : read_set) {
    const auto& prev = last_writes_[slot];
    // cout << "last write to slot performed by " << prev.tx_ << endl;
    if (prev.tx_ != LastWrite::NO_TX) {
      if (!has_dep) {
        // SUG: use upgradable locks from boost?
        read.unlock();
        write.lock();
      }
      has_dep = true;
      // cout << tx << " depends on previous tx " << prev.tx_ << endl;
      add_dep(tx, prev.tx_);
    }
  }
}

void DependencyGraph::sticky_written(Tid tx, int slot) {
  last_writes_[slot].tx_ = tx;
  last_writes_[slot].depth_++;
}

} // namespace lazy
