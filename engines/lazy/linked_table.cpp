#include "linked_table.h"
#include "logs.h"

#include <cassert>

namespace lazy {

LinkedIntColumn::LinkedIntColumn(std::vector<int>&& data) {
  data_ = std::vector<Bucket>();
  data_.reserve(data.size());
  for (std::vector<Bucket>::size_type i = 0; i < data.size(); i++) {
    // Only place where the move ctor should be called
    // since emplace requires that the type is move-ctible
    // in case of a reallocation due to a resize
    data_.emplace_back(Bucket(constants::T0, data[i]));
  }
}

LinkedIntColumn LinkedIntColumn::from_raw(int ntuples, int* data) {
  return LinkedIntColumn(std::vector<int>(data, data + ntuples));
}

void LinkedIntColumn::insert_at(int bucket, Time t, int val) {
    // Two txs are ordered wrt to the dependency graph if:
    // One reads the other's write to a slot. I.e the latter's readset
    // n former's writeset contains the given slot. 
    // However two txs which perform a blind write to a slot are not ordered
    // with respect to the timestamp ordering, therefore the insertions need to
    // be synchronised.

    cout << "node " << bucket << " ";
    data_[bucket].push(t, val);
}

LinkedTable::LinkedTable(std::vector<LinkedIntColumn>* cols): cols_(cols) {
    int tb_size = (*cols)[0].size();
    last_substantiations_ = std::vector<std::atomic<Time>>(tb_size);
    for (int i = 0; i < tb_size; i++) {
        last_substantiations_[i].store(constants::T0, std::memory_order_seq_cst);
    }
}

int LinkedIntColumn::size() const {
    return data_.size();
}

void LinkedTable::insert_at(int col, int bucket, Time t, int val) {
    (*cols_)[col].insert_at(bucket, t, val);
}

int LinkedTable::safe_read_int(int slot, int col, Time t) {
    // ------------------------------
    // When a client requests a read then it finds the latest version of a value,
    // then gets the occupied counter, then tries to safe read the value at that counter.
    auto& column = (*cols_)[col].data_;
    int64_t val;
    Entry::EntryData entry;
    Time entry_t;
    bool found = false;

    auto& bucket = column[slot];
    auto& curr = bucket.head_;
    Bucket::BucketNode* e = nullptr;
    while ((e = curr.load(std::memory_order_seq_cst)) != nullptr) {

        // If implemented as a linked list the entry need not be atomic actually...
        entry = e->entry_.load(std::memory_order_seq_cst);
        cout << "entry at slot " << slot << " has time " << entry.t_ << endl;
        if (entry.has_time(t)) {
            found = true;
            val = entry.val_;
            entry_t = entry.val_;
            break;
        }
        curr = e->next_.load(std::memory_order_seq_cst);
    }

    // If for some reason the value was not found (i.e some thread was asked to 
    // read it but the stickification thread has not even written the sticky) return
    // INT_MIN and log
    if (!found) {
      // TODO: log this somewhere (in an append-only log?)
      cout << "Entry " << slot << " not found for time " << t << " | "
          << " slot has " << Globals::table_->size_at(slot, 0) << " entries " << endl;
      return std::numeric_limits<int>::min();
    }

    // SUG: Different memory ordering
    Time last_write = last_substantiations_[slot].load(std::memory_order_seq_cst);
    cout << "entry " << slot << " found! with last write at t: " << last_write << endl;
    
    // The entry might not be a sticky but last_write < t
    // Some thread might have written to it but not updated last_substantiations
    // or two threads with different timestamp wrote to last_substantiations
    // but the one with lower time won.
    if (!entry.is_sticky() || (last_write >= t)) {
        // Nobody will ever write this slot anymore, so just 
        // find the value
        return val;
    }
    assert(entry_t < 0); // This must be a sticky! 
    // Nobody has substantiated this sticky, so let's do it ourselves.
    Tid tx_id = val;
    auto* tx = Globals::dep_.tx_of(tx_id);
    tx->substantiate();

    auto& curr_ = column[slot].head_;
    while ((e = curr_.load(std::memory_order_seq_cst)) != nullptr) {
        // If implemented as a linked list the entry need not be atomic actually...
        entry = e->entry_.load(std::memory_order_seq_cst);
        if (entry.has_time(t)) {
            return entry.val_;
        }
        curr_ = e->next_.load(std::memory_order_seq_cst);
    }
    throw std::runtime_error("unreachable");
}

void LinkedTable::raw_write_int(int slot, int col, int val, Time t, Tid as) {
    // When this is called, it is assumed that all the reads that the write depends on
    // have been executed, as well as all other dependant transactions 
    // (since the reads must have happened earlier to get the value, in which case
    // they triggered a substification chain, or this was a blind write, in which
    // case this does not matter)
    auto& column = (*cols_)[col].data_;
    auto& bucket = column[slot];
    bucket.push(t, val);
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
