#pragma once

#include <vector>
#include <mutex>

#include "lazy_engine.h"
#include "types.h"

namespace lazy {

  class Table;
    using Computation = int (*)(Table*);

  enum OperationTy {
    READ, WRITE, BIN_MUL, BIN_ADD, CONSTANT
  };

  struct ReadOp {
    int slot_;
    Time time_;
  };

  struct WriteOp {
    int slot_;
    Time time_;
  };

  struct ConstantOp {
    int value_;
  };

  struct BinAddOp { };
  struct BinMulOpp { };

  struct Operation {
    OperationTy ty_;
    union {
      ReadOp read_;
      WriteOp write_;
      int bin_add_;
      int constant_;
    } op_;

    bool is_read() const;
    bool is_write() const;
    int read_slot() const;
    int write_slot() const;
  };


  class Request {
    public:
      using Tid = int;


      // SUG: Heuristic for how many slots would be a read or write so we can
      // pre-allocate
      Request(bool is_tx, Computation code, std::vector<Operation>&& ops): is_tx_(is_tx), operations_(std::move(ops)),  fp_(code), rw_known_in_advance_(false) {
        set_request_time();
      }

      Request(bool is_tx, Computation code, std::vector<Operation>&& ops, std::vector<int>&& write_set, std::vector<int>&& read_set): is_tx_(is_tx), operations_(std::move(ops)), fp_(code), rw_known_in_advance_(true), read_set_(std::move(read_set)) , write_set_(std::move(write_set)) {
      set_request_time();
    }

      /* Read the operations and decide the read-set and write-set of a transaction */
      void stickify();
      void substantiate();

    private:
      void insert_sticky(int slot);
      void set_request_time();

      static Tid request_cnt;

      // Transaction, or just normal request?
      bool is_tx_; 
    // sorted in prefix notation to avoid recursive types
      // how about recursive type with allocated arena?
      std::vector<Operation> operations_;
      Computation fp_;
      // For testing purposes assume the r/w sets have been determined
      // without the overhead of interpretation
      bool rw_known_in_advance_;
      std::vector<int> read_set_; // slots, but at what time?
      std::vector<int> write_set_; // slots, but at what time?
      Tid tid_; // slots, but at what time?
      Time epoch_; // slots, but at what time?

      // Lock for the corresponding tx. In the initial implementation, only
      // 1 thread is a allowed to substantiate this transaction at a time.
      std::mutex tx_lock_;
  };

} // namespace lazy


