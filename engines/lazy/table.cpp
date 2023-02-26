#include <atomic>
#include <cassert>

#include "table.h"
#include "lazy_engine.h"
#include "types.h"

namespace lazy {

using std::atomic;

IntSlot IntSlot::sticky(Time t, Tid tid) {
    return IntSlot(-t, tid);
}

int Entry::get_value(std::memory_order ord) {
  int64_t entry = data_.load(ord);
  int val_mask = 0xffffffff;
  return static_cast<int>(entry & val_mask);
}

Entry Entry::sticky(Time t, int val) {
  return Entry(-t, val);
}

inline Tid Entry::get_transaction_id(std::memory_order ord) {
  return static_cast<Tid>(get_value(ord));
}

inline Time Entry::sticky_time(std::memory_order ord) {
  return -1 * write_time(ord);
}

Time Entry::write_time(std::memory_order ord) {
  return static_cast<Time>(data_.load(ord) >> 31);
}

void Entry::write(Time t, int val, std::memory_order ord) {
  int64_t data = (t << 31) | val;
  data_.store(data, ord);
}

int64_t Entry::load(std::memory_order ord) {
  return data_.load(ord);
}

bool Entry::is_invalid(std::memory_order ord) {
  int64_t entry = data_.load(ord);
  int64_t invalid_mask = constants::T_INVALID << 31;
  int64_t time = invalid_mask & entry;
  return time == invalid_mask;
}

bool Entry::has_time(int64_t entry, Time t) {
  // Precondition: Not to be called with INT64_MIN
  int64_t entry_t = entry >> 31; 
  return entry_t == t || entry_t == -t;
}

int Entry::get_val(int64_t entry) {
  // Precondition: Not to be called with INT64_MIN
  int64_t entry_val = entry & (0xffffffff << 31);
  return static_cast<int>(entry_val);
}

inline bool Entry::is_sticky(int64_t entry) {
  return (entry >> 31) < 0;
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

void IntColumn::insert_at(int bucket, IntSlot&& val) {
    auto it = bucket * constants::TIMESTAMPS_PER_TUPLE;
    auto end = it + constants::TIMESTAMPS_PER_TUPLE;
    bool found_free = false;
    for (; it < end; it++) {
        // TODO: use Entry class.
        if (data_[it].is_invalid(std::memory_order_seq_cst)) {
            // TODO: Eviction policy?
            data_[it].write(val.t_, val.val_, std::memory_order_seq_cst);
            found_free = true;
            break;
        }
    }
    if (!found_free) {
        // filled up the time versions, throw.
        throw std::runtime_error("Object already has 5 time-versions");
    }
}

Table::Table(std::vector<IntColumn>&& cols): cols_(std::move(cols)) {
    last_substantiations_ = std::vector<std::atomic<Time>>(cols.size());
}

void Table::insert_at(int col, int bucket, IntSlot&& val) {
    cols_[col].insert_at(bucket, std::move(val));
}

inline int Table::safe_read_int(int slot, int col, Time t) {
    // ------------------------------
    // When a client requests a read then it finds the latest version of a value,
    // then gets the occupied counter, then tries to safe read the value at that counter.


    auto& column = cols_[col].data_;
    
    auto it = slot * constants::TIMESTAMPS_PER_TUPLE;
    auto end = it + constants::TIMESTAMPS_PER_TUPLE;
    int64_t val;
    int64_t entry;
    int64_t entry_t;
    bool found = false;
    int found_idx = -1;
    for (; it < end; it++) {
        // SUG: Different memory ordering
        entry = column[it].load(std::memory_order_seq_cst);
        if (Entry::has_time(entry, t)) {
            found = true;
            val = Entry::get_val(entry);
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
    if (!Entry::is_sticky(entry) || (last_write >= t)) {
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

} // namespace lazy
