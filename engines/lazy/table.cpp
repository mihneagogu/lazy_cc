#include <atomic>
#include <cassert>

#include "table.h"
#include "logs.h"
#include "lazy_engine.h"
#include "types.h"

namespace lazy {

using std::atomic;

IntSlot IntSlot::sticky(Time t, Tid tid) {
    return IntSlot(-t, tid);
}

Entry::Entry(Time t, int val): detail_(EntryData(t, val)) {}

int Entry::get_value(std::memory_order ord) {
  auto detail = detail_.load(ord);
  return detail.val_;
}

inline Tid Entry::get_transaction_id(std::memory_order ord) {
  return static_cast<Tid>(get_value(ord));
}

inline Time Entry::sticky_time(std::memory_order ord) {
  return -1 * write_time(ord);
}

Time Entry::write_time(std::memory_order ord) {
  return detail_.load(ord).t_;
}

void Entry::write(Time t, int val, std::memory_order ord) {
  detail_.store(EntryData(t, val), ord);
}

Entry::EntryData Entry::load(std::memory_order ord) {
  return detail_.load(ord);
}

bool Entry::EntryData::is_invalid() const {
  return t_ == constants::T_INVALID;
}

bool Entry::EntryData::has_time(Time t) const {
  return t_ == t || t_ == -t;
}

inline bool Entry::EntryData::is_sticky() const {
  return t_ < 0;
}


IntColumn::IntColumn(std::vector<int>&& data) {
  data_ = std::vector<Entry>(data.size() * constants::TIMESTAMPS_PER_TUPLE);
  for (std::vector<Entry>::size_type i = 0; i < data.size(); i++) {
    data_[constants::TIMESTAMPS_PER_TUPLE * i].write(constants::T0, data[i], std::memory_order_seq_cst);
  }
}

IntColumn IntColumn::from_raw(int ntuples, int* data) {
  return IntColumn(std::vector<int>(data, data + ntuples));
}

void IntColumn::insert_at(int bucket, Time t, int val) {
    auto it = bucket * constants::TIMESTAMPS_PER_TUPLE;
    auto end = it + constants::TIMESTAMPS_PER_TUPLE;
    bool found_free = false;
    for (; it < end; it++) {
        // TODO: use Entry class.
        auto ed = data_[it].load(std::memory_order_seq_cst);
        if (ed.is_invalid()) {
            // TODO: Eviction policy?
            data_[it].write(t, val, std::memory_order_seq_cst);
            found_free = true;
            break;
        }
    }
    if (!found_free) {
        // filled up the time versions, throw.
        throw std::runtime_error("Object already has 5 time-versions");
    }
}

Table::Table(std::vector<IntColumn>* cols): cols_(cols) {
    last_substantiations_ = std::vector<std::atomic<Time>>(cols->size());
}

void Table::insert_at(int col, int bucket, Time t, int val) {
    (*cols_)[col].insert_at(bucket, t, val);
}

int Table::safe_read_int(int slot, int col, Time t) {
    // ------------------------------
    // When a client requests a read then it finds the latest version of a value,
    // then gets the occupied counter, then tries to safe read the value at that counter.


    auto& column = (*cols_)[col].data_;
    
    auto it = slot * constants::TIMESTAMPS_PER_TUPLE;
    auto end = it + constants::TIMESTAMPS_PER_TUPLE;
    int64_t val;
    Entry::EntryData entry;
    Time entry_t;
    bool found = false;
    int found_idx = -1;
    for (; it < end; it++) {
        // SUG: Different memory ordering
        entry = column[it].load(std::memory_order_seq_cst);
        if (entry.has_time(t)) {
            found = true;
            val = entry.val_;
            found_idx = it;
            break;
        }
    }

    // If for some reason the value was not found (i.e some thread was asked to 
    // read it but the stickification thread has not even written the sticky) return
    // INT_MIN and log
    if (!found) {
      // TODO: log this somewhere (in an append-only log?)
      return std::numeric_limits<int>::min();
    }

    // SUG: Different memory ordering
    Time last_write = last_substantiations_[slot].load(std::memory_order_seq_cst);
    
    // The entry might not be a sticky but last_write < t
    // Some thread might have written to it but not updated last_substantiations
    // or two threads with different timestamp wrote to last_substantiations
    // but the one with lower time won.
    if (!entry.is_sticky() || (last_write >= t)) {
        // Nobody will ever write this slot anymore, so just 
        // find the value
        return val;
    }
    assert(entry_t < 0); // Time 
    // Nobody has substantiated this sticky, so let's do it ourselves.
    Tid tx_id = val;
    auto* tx = Globals::dep_.tx_of(tx_id);
    tx->substantiate();
    return column[found_idx].get_value(std::memory_order_seq_cst);
}

void Table::raw_write_int(int slot, int col, int val, Time t, Tid as) {

}


void Table::enforce_wirte_set_substantiation(Time new_time, const std::vector<int>& write_set) {
  for (auto slot : write_set) {
    Time before = last_substantiations_[slot].load(std::memory_order_seq_cst);
    Time best_time = std::max(before, new_time);
    // Note, this is a best-effort approach. We try our best to put the max slot in, but it's possible that a third thread performs this operation before our load and cas, therefore making the cas fail. We could ensure the highest slot by doing a CAS loop, however. last_substantiations is used as a heuristic, nonetheless, so it seems unnecessary
    /* bool _succ = */ last_substantiations_[slot].compare_exchange_strong(before, best_time, std::memory_order_seq_cst);
  }

}

Table::~Table() {
  if (cols_) {
    delete cols_;
  }
}

} // namespace lazy
