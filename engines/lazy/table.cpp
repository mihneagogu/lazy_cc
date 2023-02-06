#include "table.h"

namespace lazy {

IntSlot IntSlot::sticky(Time t, Tid tid) {
    return IntSlot(-t, tid);
}

bool IntSlot::is_invalid() const {
    return t_ == constants::T_INVALID;
}

IntColumn::IntColumn(std::vector<int> data) {
  data_.resize(data.size() * constants::TIMESTAMPS_PER_TUPLE);
  for (int i = 0; i < data.size(); i++) {
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
    for (; it < end; start++) {
        if (data_[it].is_invalid()) {
            // found a free slot
            data_[it] = std::move(val);
            found_free = true;
            break;
        }
    }
    if (!found_free) {
        // filled up the time versions, throw.
        throw TimeversionsFullException();
    }
}

void Table::insert_at(int col, int bucket, IntSlot&& val) {
    cols_[col].insert_at(bucket, std::move(val));
}

} // namespace lazy
