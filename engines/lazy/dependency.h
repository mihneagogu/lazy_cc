#pragma once

#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>

// #include "request.h"
#include "types.h"

namespace lazy {

class Request;

struct LastWrite {
  static constexpr Tid NO_TX = -1;
  Tid tx_;
  int depth_;
  LastWrite(): tx_(NO_TX), depth_(0) {}
  LastWrite(Tid tx, int depth): tx_(tx), depth_(depth) {}
  bool was_written() const;
};

class DependencyGraph {
  public:
    DependencyGraph(int n_slots) {
      last_writes_ = std::vector<LastWrite>(n_slots);
    }
    /* Must be called without holding the lock. Will acquire a shared lock
     * and upgrade to exclusive in  case there is indeed a dependency */
    void check_dependencies(Tid tx, const std::vector<int>& read_set);
    std::vector<Request*> get_dependencies(Tid of);
    Request* tx_of(Tid tid);

    void sticky_written(Tid tx, int slot);

    mutable std::shared_mutex global_lock_;
    std::unordered_map<Tid, std::vector<Request*>> dependencies_;
    std::unordered_map<Tid, Request*> txs_;
  private:
    void add_dep(Tid tx, Tid on);
    std::vector<LastWrite> last_writes_;
};

// each transaction has its own dependency structure inside of the trans.
// on each write we update the "last written to datastructure" which contains 
// for each slot the number of the last writer (which is a sticky)

// What if T2 is executed after T1, and reads a,b. And T1 writes a,b.
// T1 when stickified sees that T1 writes a. Then T2 move to b. T1 gets executed.
// However T1 was still the last to write T 1. If somehow T2 gets subst.
// and tries to execute T1, it's no problem, it was simply executed in the meantime, it
// does not matter

} // namespace lazy
