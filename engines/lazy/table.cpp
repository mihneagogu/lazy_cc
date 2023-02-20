#include "table.h"
#include "types.h"

namespace lazy {

using std::atomic;

IntSlot IntSlot::sticky(Time t, Tid tid) {
    return IntSlot(-t, tid);
}

bool IntSlot::is_invalid() const {
    return t_ == constants::T_INVALID;
}

IntColumn::IntColumn(std::vector<int> data) {
  data_.resize(data.size() * constants::TIMESTAMPS_PER_TUPLE);
  for (std::vector<int>::size_type i = 0; i < data.size(); i++) {
    data_[constants::TIMESTAMPS_PER_TUPLE * i] = IntSlot(constants::T0, data[i]);
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
        // TODO: use occupied counter
        if (data_[it].is_invalid()) {
            // TODO: Eviction policy?
            data_[it] = std::move(val);
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
    auto& pos = cols_[col].data_[slot * constants::TIMESTAMPS_PER_TUPLE];
    
    // SUG: Different memory ordering
    Time last_write = last_substantiations_[slot].load(std::memory_order_seq_cst);
    if (last_write > t) {
        // Nobody will ever write this slot anymore, so just 
        // read the value
    }
    return 0;
}

inline void Table::safe_write_int(int slot, int col, int val, Time t) {

}


} // namespace lazy
