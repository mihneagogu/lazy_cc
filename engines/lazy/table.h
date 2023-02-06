#pragma once

#include "lazy_engine.h"
#include "types.h"

namespace lazy {

  /*
    If t > 0, then represents (time of write, value),
    otherwise represents (-time of sticky insertion, transaction id)
  */
  struct IntSlot {
    IntSlot(Time time, int value): t_(time), val_(value) {}
    IntSlot(): t_(constants::T_INVALID) {}
    static IntSlot sticky(Time t, Transaction::Tid tid);

    Time t_;
    int val_;
    bool is_invalid() const;
    // bool is_sticky() const;
    // int get_transaction_id() const;
  };

  class TimeversionsFullException : public std::exception {
    public:
      char* what() {
        return "Timeseries is full for a certain slot";
      }
  };

  class IntColumn {
    public:
      IntColumn(int ntuples): ntuples_(ntuples) {
        data_.reserve(constants::TIMESTAMPS_PER_TUPLE * ntuples);
      }
      IntColumn(std::vector<int> data);
      static IntColumn from_raw(int ntuples, int* data);

      void insert_at(int bucket, IntSlot&& val);
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

  class Table {
    public:
        Table() = default;
        Table(std::vector<IntColumn>&& cols): cols_(std::move(cols)) {}
        int rows() const;
        void insert_at(int col, int bucket, IntSlot&& val);
    private:
      std::vector<IntColumn> cols_;
  };

} // namespace lazy
