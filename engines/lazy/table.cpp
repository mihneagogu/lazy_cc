#include "table.h"

namespace lazy {

IntColumn::IntColumn(std::vector<int> data) {
  data_.resize(data.size() * constants::TIMESTAMPS_PER_TUPLE);
  for (int i = 0; i < data.size(); i++) {
    data_[constants::TIMESTAMPS_PER_TUPLE * i] = IntSlot(constants::T0, data[i]);
  }
}

IntColumn IntColumn::from_raw(int ntuples, int* data) {
  std::vector<int> vec_data(data, data + ntuples);
  return IntColumn(std::move(vec_data));
}

} // namespace lazy
