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

  /*
    If t > 0, then represents (time of write, value),
    otherwise represents (-time of sticky insertion, transaction id)
  */
  struct IntSlot {
    static_assert(sizeof(int) == 4, "Assuming that the size of an int is 4");

    IntSlot(Time time, int value): t_(time), val_(value) {}
    IntSlot(): t_(constants::T_INVALID) {}
    static IntSlot sticky(Time t, Tid tid);
    Time t_;
    int val_;
    bool is_invalid() const;
    bool is_sticky() const;
    int get_transaction_id() const;
  };

  // Represents a pair of 2 int32s
  // If time > 0, then represents (time of write, value),
  // otherwise represents (-time of sticky insertion, transaction id)
  class Entry {
    public:
      Entry() = default;
      Entry(const Entry& other) = delete;
      Entry(Entry&& other) = delete;
      Entry(Time t, int val): data_((static_cast<int64_t>(t) << 31) | val) {}

      static Entry sticky(Time t, int val);
      bool is_invalid(std::memory_order ord = std::memory_order_seq_cst);
      Tid get_transaction_id(std::memory_order ord = std::memory_order_seq_cst);
      int get_value(std::memory_order ord = std::memory_order_seq_cst);
      Time sticky_time(std::memory_order ord = std::memory_order_seq_cst);
      Time write_time(std::memory_order ord = std::memory_order_seq_cst);

      void write(Time t, int val, std::memory_order ord = std::memory_order_seq_cst);
      int64_t load(std::memory_order ord = std::memory_order_seq_cst);

      static bool has_time(int64_t entry, Time t);
      static bool is_sticky(int64_t entry);
      static int get_val(int64_t entry);
      static int get_time(int64_t entry);
    private:
      std::atomic<int64_t> data_;
  };

  class IntColumn {
    public:
      IntColumn(std::vector<int>&& data);
      static IntColumn from_raw(int ntuples, int* data);

      void insert_at(int bucket, IntSlot&& val);

      // What is the logical capacity of records of this
      int ntuples_;

      // How many latest-version records are we storing? (either sticky or not)
      int occupied_;

      // How many slots are occupied (a record with x written-to timestamps occupies x actual size slots)
      int actual_size_; 
      
      // Worth using manual allocation to avoid default ctor being called everywhere?
      std::vector<Entry> data_;
  };

  class Table {
    public:
        Table() = default;
        Table(std::vector<IntColumn>* cols);
        int rows() const;
        void insert_at(int col, int bucket, IntSlot&& val);
        
        int safe_read_int(int slot, int col, Time t);
        void safe_write_int(int slot, int col, int val, Time t);

        int raw_read_int(int slot, int col, Time t, Tid as);
        // TODO: when calling raw_write_int also increment the last_substantiations_
        void raw_write_int(int slot, int col, int val, Time t, Tid as);
        void enforce_wirte_set_substantiation(Time new_time, const std::vector<int>& write_set);

      ~Table();
    private:
      std::vector<IntColumn>* cols_;

      // SUG: Move this and IntColumn into the same allocation
      // The value in last_substantiations guarantees that the last substantiation
      // has happened at least at a time >= the value.
      // If last_substntiations_[slot] < t, the substantiation may have actually
      // happened and the executing thread either
      // 1) Did not get to write to the vector yet. Or
      // 2) Two threads were racing to write to it, and the one with lower
      // time happened to write to it later.
      //
      // This is a best-effort construct
      std::vector<std::atomic<Time>> last_substantiations_;
      // TODO free cols
  };

} // namespace lazy
