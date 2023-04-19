#include <atomic>
#include <cassert>

#include "entry.h"
#include "logs.h"
#include "lazy_engine.h"
#include "types.h"

namespace lazy {

using std::atomic;

Entry::Entry(Time t, int val): detail_(EntryData(t, val)) {}

int Entry::get_value(std::memory_order ord) {
  auto detail = detail_.load(ord);
  return detail.val_;
}

Tid Entry::get_transaction_id(std::memory_order ord) {
  return static_cast<Tid>(get_value(ord));
}

Time Entry::sticky_time(std::memory_order ord) {
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

} // namespace lazy
