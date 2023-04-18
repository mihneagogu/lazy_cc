#pragma once

#include <string>
#include <stdexcept>
#include <atomic>
#include <cstdint>
#include <list>

#include "lazy_engine.h"
#include "request.h"
#include "types.h"

namespace lazy {

  // Represents a pair of 2 int32s
  // If time > 0, then represents (time of write, value),
  // otherwise represents (-time of sticky insertion, transaction id)
  class Entry {
    // Does this really need to be atomic?
    // Can a thread read this value while we are writing to it?
    public:
      struct EntryData {
        Time t_;
        int val_;
        EntryData() = default;
        EntryData(Time t, int val): t_(t), val_(val) {}

        bool has_time(Time t) const;
        bool is_sticky() const;
        bool is_invalid() const;
      };

      Entry() = default;
      Entry(const Entry& other) = delete;
      Entry(Entry&& other) = delete;
      Entry(Time t, int val);

      static Entry sticky(Time t, int val);
      Tid get_transaction_id(std::memory_order ord = std::memory_order_seq_cst);
      int get_value(std::memory_order ord = std::memory_order_seq_cst);
      Time sticky_time(std::memory_order ord = std::memory_order_seq_cst);
      Time write_time(std::memory_order ord = std::memory_order_seq_cst);

      void write(Time t, int val, std::memory_order ord = std::memory_order_seq_cst);
      Entry::EntryData load(std::memory_order ord = std::memory_order_seq_cst);

    private:
      std::atomic<EntryData> detail_;
  };

} // namespace lazy
