#pragma once

#include "lazy_engine.h"

namespace lazy {

  class Table {

  };

  /*
    If t > 0, then represents (time of write, value),
    otherwise represents (-time of sticky insertion, transaction id)
  */
  struct IntSlot {
    IntSlot(Time t, int value): t_(t), val_(value) {}
    IntSlot(): t_(constants::T_INVALID) {}
    Time t_;
    int val_;
    // bool is_sticky() const;
    // int get_transaction_id() const;
  };


  class IntColumn {
    public:
      IntColumn(int ntuples): ntuples_(ntuples) {
        data_.reserve(constants::TIMESTAMPS_PER_TUPLE * ntuples);
      }
      static IntColumn from_raw(int ntuples, int* data);
      IntColumn(std::vector<int> data);
    private:

      // What is the logical capacity of records of this
      int ntuples_;

      // How many latest-version records are we storing? (either sticky or not)
      int occupied_;

      // How many slots are occupied (a record with x written-to timestamps occupies x actual size slots)
      int actual_size_; 
      
      // Worth using manual allocation to avoid default ctor being called everywhere?
      std::vector<IntSlot> data_;
  };

} // namespace lazy
