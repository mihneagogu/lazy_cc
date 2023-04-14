#pragma once

#include <string>
#include <mutex>
#include <stdexcept>
#include <atomic>
#include <cstdint>
#include <list>
#include <optional>
#include <cassert>

#include "lazy_engine.h"
#include "logs.h"
#include "request.h"
#include "types.h"
#include "table.h"

namespace lazy {

  struct Bucket {

    // TODO: Make an iterator interface
    struct BucketNode {
      Entry entry_; 
      std::atomic<BucketNode*> next_;
      
      BucketNode(Time t, int val): entry_(t, val), next_(nullptr) {}

      ~BucketNode() {
        auto* next = next_.load();
        if (next) {
          delete next;
        }
      }
    };

    Bucket(Time t, int val) {
      auto* node = new BucketNode(t, val);
      head_ = node;
      tail_ = node;
      size_.store(1);
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
      size_.store(other.size());
    }
    Bucket(const Bucket& other) = delete;

    std::atomic<BucketNode*> head_;
    std::atomic<BucketNode*> tail_;
    std::atomic<int> size_;

    void push(Time t, int val) {
      auto* node = new BucketNode(t, val);
      push(node);
      size_.fetch_add(1);
    }

    int size() const {
      return size_.load();
    }

    void write_at(Time t, int val) {
      Bucket::BucketNode* e = head_.load(std::memory_order_seq_cst);
      Entry::EntryData entry;
      while (e != nullptr) {
          entry = e->entry_.load(std::memory_order_seq_cst);
          cout << "write entry with time " << entry.t_ << endl;
          if (entry.has_time(t)) {
              e->entry_.write(t, val, std::memory_order_seq_cst);
              cout << "written to entry new value " << endl;
              return;
          }
          e = e->next_.load(std::memory_order_seq_cst);
      }
      throw std::runtime_error("Trying to write to an entry at a time which doesn't exist");
    }
  
    int latest_value() {
      Bucket::BucketNode* e = head_.load(std::memory_order_seq_cst);
      Entry::EntryData entry;
      Time latest = 0;
      int val = 1;
      while (e != nullptr) {
          entry = e->entry_.load(std::memory_order_seq_cst);
          if (entry.t_ < 0) {
            cout << "entry has t < 0: " << entry.t_ << endl;
          }
          assert(entry.t_ > 0);
          if (entry.t_ > latest) {
            latest = entry.t_;
            val = entry.val_;
          }
          e = e->next_.load(std::memory_order_seq_cst);
      }
      return val; 
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

    ~Bucket() {
      auto* head = head_.load();
      if (head) {
        delete head;
      }
    }
  };

  class LinkedIntColumn {
    public:
      LinkedIntColumn(std::vector<int>&& data);
      int size() const;
      static LinkedIntColumn from_raw(int ntuples, int* data);

      void insert_at(int bucket, Time t, int val);

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
        void insert_at(int col, int bucket, Time t, int val);
        
        int safe_read_int(int slot, int col, Time t);
        void safe_write_int(int slot, int col, int val, Time t);

        // TODO remove
        int size_at(int slot, int col) {
          return (*cols_)[0].data_[slot].size();
        }
        void safe_write_int(int slot, int col, int val, Time t, Tid as);
        void enforce_wirte_set_substantiation(Time new_time, const std::vector<int>& write_set);

        int checksum();

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
