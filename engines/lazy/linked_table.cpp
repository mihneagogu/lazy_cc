#include "linked_table.h"

#include <cassert>

namespace lazy {

LinkedIntColumn::LinkedIntColumn(std::vector<int>&& data) {
  data_ = std::vector<std::list<Entry>>(data.size());
  for (std::vector<std::list<Entry>>::size_type i = 0; i < data.size(); i++) {
    data_[i].emplace_front(constants::T0, data[i]);
  }
}

LinkedIntColumn LinkedIntColumn::from_raw(int ntuples, int* data) {
  return LinkedIntColumn(std::vector<int>(data, data + ntuples));
}

void LinkedIntColumn::insert_at(int bucket, IntSlot&& val) {
    // Two txs are ordered wrt to the dependency graph if:
    // One reads the other's write to a slot. I.e the latter's readset
    // n former's writeset contains the given slot. 
    // However two txs which perform a blind write to a slot are not ordered
    // with respect to the timestamp ordering, therefore the insertions need to
    // be synchronised.

    locks_[bucket].lock();

    data_[bucket].emplace_back(val.t_, val.val_);

    locks_[bucket].unlock();
}

LinkedTable::LinkedTable(std::vector<LinkedIntColumn>* cols): cols_(cols) {
    last_substantiations_ = std::vector<std::atomic<Time>>(cols->size());
}

void LinkedTable::insert_at(int col, int bucket, IntSlot&& val) {
    (*cols_)[col].insert_at(bucket, std::move(val));
}

int LinkedTable::safe_read_int(int slot, int col, Time t) {
    // ------------------------------
    // When a client requests a read then it finds the latest version of a value,
    // then gets the occupied counter, then tries to safe read the value at that counter.


    auto& column = (*cols_)[col].data_;

    auto& lock = (*cols_)[col].locks_[slot];
    lock.lock();
    
    int64_t val;
    int64_t entry;
    int64_t entry_t;
    bool found = false;

    for (auto& et : column[slot]) {
        // SUG: Different memory ordering
        entry = et.load(std::memory_order_seq_cst);
        if (Entry::has_time(entry, t)) {
            found = true;
            val = Entry::get_val(entry);
            break;
        }
    }

    // If for some reason the value was not found (i.e some thread was asked to 
    // read it but the stickification thread has not even written the sticky) return
    // INT_MIN and log
    if (!found) {
      // TODO: log this somewhere (in an append-only log?)
      lock.unlock();
      return std::numeric_limits<int>::min();
    }

    // SUG: Different memory ordering
    Time last_write = last_substantiations_[slot].load(std::memory_order_seq_cst);
    
    // The entry might not be a sticky but last_write < t
    // Some thread might have written to it but not updated last_substantiations
    // or two threads with different timestamp wrote to last_substantiations
    // but the one with lower time won.
    if (!Entry::is_sticky(entry) || (last_write >= t)) {
        // Nobody will ever write this slot anymore, so just 
        // find the value
        lock.unlock();
        return val;
    }
    assert(entry_t < 0); // Time 
    // Nobody has substantiated this sticky, so let's do it ourselves.
    Tid tx_id = val;
    auto* tx = Globals::dep_.tx_of(tx_id);
    lock.unlock();
    tx->substantiate();

    lock.lock();
    for (auto& et : column[slot]) {
        // SUG: Different memory ordering
        entry = et.load(std::memory_order_seq_cst);
        if (Entry::has_time(entry, t)) {
            val = Entry::get_val(entry);
            lock.unlock();
            return val;
        }
    }
    throw std::runtime_error("unreachable");
}

void LinkedTable::raw_write_int(int slot, int col, int val, Time t, Tid as) {

}


void LinkedTable::enforce_wirte_set_substantiation(Time new_time, const std::vector<int>& write_set) {
  for (auto slot : write_set) {
    Time before = last_substantiations_[slot].load(std::memory_order_seq_cst);
    Time best_time = std::max(before, new_time);
    // Note, this is a best-effort approach. We try our best to put the max slot in, but it's possible that a third thread performs this operation before our load and cas, therefore making the cas fail. We could ensure the highest slot by doing a CAS loop, however. last_substantiations is used as a heuristic, nonetheless, so it seems unnecessary
    /* bool _succ = */ last_substantiations_[slot].compare_exchange_strong(before, best_time, std::memory_order_seq_cst);
  }

}

LinkedTable::~LinkedTable() {
  if (cols_) {
    delete cols_;
  }
}

}
