#pragma once

#include <string>
#include <mutex>
#include <stdexcept>
#include <atomic>
#include <cstdint>
#include <list>

#include "lazy_engine.h"
#include "request.h"
#include "types.h"
#include "table.h"

namespace lazy {

  struct Bucket {

    struct BucketNode {
      Entry entry_; 
      std::atomic<BucketNode*> next_;
      
      BucketNode(Time t, int val): entry_(t, val), next_(nullptr) {}
    };

    Bucket(Time t, int val) {
      auto* node = new BucketNode(t, val);
      head_ = node;
      tail_ = node;
    }

    Bucket() = delete;
    Bucket(Bucket&& other) {
      // TO NEVER BE USED OUTSIDE OF TABLE INITIALIZATION.
      // in LinkedIntColumn::LinkedIntColumn(std::vector<int>&& data)
      // Comment in constructor. Otherwise should NEVER be used
      head_.store(other.head_.load());
      tail_.store(other.tail_.load());
      other.head_.store(nullptr);
      other.tail_.store(nullptr);
    }
    Bucket(const Bucket& other) = delete;

    std::atomic<BucketNode*> head_;
    std::atomic<BucketNode*> tail_;

    void push(Time t, int val) {
      push(new BucketNode(t, val));
    }
    void push(BucketNode* e) {
      auto prev_tail = tail_.load(std::memory_order_seq_cst);
      while (!tail_.compare_exchange_strong(prev_tail, e, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
        // Backoff?
        // Relaxed loads to avoid contention?
        prev_tail = tail_.load(std::memory_order_seq_cst);
      }
      prev_tail->next_.store(e, std::memory_order_seq_cst);
    }
  };

  class LinkedIntColumn {
    public:
      LinkedIntColumn(std::vector<int>&& data);
      static LinkedIntColumn from_raw(int ntuples, int* data);

      void insert_at(int bucket, IntSlot&& val);

      // What is the logical capacity of records of this
      int ntuples_;

      // How many latest-version records are we storing? (either sticky or not)
      int occupied_;

      // How many slots are occupied (a record with x written-to timestamps occupies x actual size slots)
      int actual_size_; 
      
      // Worth using manual allocation to avoid default ctor being called everywhere?
      std::vector<Bucket> data_;
  };

  class LinkedTable {
    public:
        LinkedTable() = default;
        LinkedTable(std::vector<LinkedIntColumn>* cols);
        int rows() const;
        void insert_at(int col, int bucket, IntSlot&& val);
        
        int safe_read_int(int slot, int col, Time t);
        void safe_write_int(int slot, int col, int val, Time t);

        int raw_read_int(int slot, int col, Time t, Tid as);
        // TODO: when calling raw_write_int also increment the last_substantiations_
        void raw_write_int(int slot, int col, int val, Time t, Tid as);
        void enforce_wirte_set_substantiation(Time new_time, const std::vector<int>& write_set);

      ~LinkedTable();
    private:
      std::vector<LinkedIntColumn>* cols_;

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
