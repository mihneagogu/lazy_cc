#pragma once

#include <vector>

#include "lazy_engine.h"
#include "types.h"

namespace lazy {

  // typedef int (*TransactionComputation)(Table&);
  using Computation = int;

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
      Request(std::vector<Operation>&& ops): operations_(std::move(ops)) {
        tid_ = ++request_cnt;
        epoch_ = Globals::clock_.advance();
      }


      /* Read the operations and decide the read-set and write-set of a transaction */
      void stickify();
      /* Simply execute the code of the transaction */
      void substantiate();

    private:
      void insert_sticky(int slot);

      static Tid request_cnt;

      // sorted in prefix notation to avoid recursive types
      // how about recursive type with allocated arena?
      std::vector<Operation> operations_;
      Computation fp_;
      std::vector<int> read_set_; // slots, but at what time?
      std::vector<int> write_set_; // slots, but at what time?
      Tid tid_; // slots, but at what time?
      Time epoch_; // slots, but at what time?
      bool is_trans_ = false;
  };

} // namespace lazy


