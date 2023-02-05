#pragma once

#include <vector>

#include "table.h"

namespace lazy {

  using TransactionComputation = int(Table&);

  enum OperationTy {
    READ, WRITE, BIN_MUL, BIN_ADD, CONSTANT
  };

  struct ReadOp {
    int slot_;
    int time_;
  };

  struct WriteOp {
    int slot_;
    int time_;
  };

  struct ConstantOp {
    int value_;
  };

  struct BinAddOp { };
  struct BinMulOpp { };

  class Operation {
    OperationTy ty_;
    union {
      int read_;
      int write_;
      int bin_add_;
      int constant_;
    } op_;
  };

  class Transaction {
    public:
      Transaction(std::vector<Operation>&& ops): operations_(std::move(ops)) {}

      /* Read the operations and decide the read-set and write-set of a transaction */
      void stickify();
      /* Simply execute the code of the transaction */
      void substantiate();

    private:
      // sorted in prefix notation to avoid recursive types
      // how about recursive type with allocated arena?
      std::vector<Operation> operations_;
      TransactionComputation fp_;
      std::vector<int> read_set_; // slots, but at what time?
      std::vector<int> write_set_; // slots, but at what time?
  };

} // namespace lazy


