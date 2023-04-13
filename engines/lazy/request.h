#pragma once

#include <vector>
#include <mutex>
#include <atomic>

#include "lazy_engine.h"
#include "types.h"

namespace lazy {

  class Request;
  class LinkedTable;
    using Computation = int (*)(Request*, LinkedTable*, int /* write_1*/, int /* write_2 */, int /* write 3 */);

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



	enum class SubstantiateResult {
		SUCCESS, FAIL, STALLED, RUNNING
	};

  enum class ExecutionStatus {
    DONE, EXECUTING_NOW, UNEXECUTED
  };

  class Request {
    public:
      using Tid = int;

      // SUG: Heuristic for how many slots would be a read or write so we can
      // pre-allocate
      Request(bool is_tx, Computation code, std::vector<Operation>&& ops): is_tx_(is_tx), operations_(std::move(ops)),  fp_(code), rw_known_in_advance_(false) , stickified_(false), status_(ExecutionStatus::UNEXECUTED) {
        set_request_time();
      }

      Request(bool is_tx, Computation code, std::vector<Operation>&& ops, std::vector<int>&& write_set, std::vector<int>&& read_set): is_tx_(is_tx), operations_(std::move(ops)), fp_(code), rw_known_in_advance_(true), read_set_(std::move(read_set)) , write_set_(std::move(write_set)), stickified_(false),  status_(ExecutionStatus::UNEXECUTED) {
      set_request_time();
    }

      /* Read the operations and decide the read-set and write-set of a transaction */
      void stickify();
      SubstantiateResult substantiate();
      bool was_performed() const;
      ExecutionStatus execution_status() const;
      bool is_being_executed() const;
      Time time() const;
      Tid tx_id() const;

      void set_write_to(int slot1, int slot2, int slot3) {
        write1_ = slot1; 
        write2_ = slot2; 
        write3_ = slot3; 
        rw_known_in_advance_ = true;
      }

    int write1_;
    int write2_;
    int write3_;

    Time read1_t_;
    Time read2_t_;
    Time read3_t_;

    private:

      void insert_sticky(int slot);
      void set_request_time();

      static Tid request_cnt;

      // Transaction, or just normal request?
      bool is_tx_; 
      std::vector<Operation> operations_; // UNUSED for now since the writes and reads are hardcored

      Computation fp_;
      // For testing purposes assume the r/w sets have been determined
      // without the overhead of interpretation
      bool rw_known_in_advance_;
      std::vector<int> read_set_; 
      std::vector<int> write_set_; 
      Tid tid_; // This request's id
      Time epoch_; // commit & execution time of the transaction

      // Lock for the actualy computation execution of the transaction. 
      // Acquired when a transaction is substantiated
      std::mutex tx_lock_;
      std::atomic<bool> stickified_;
      std::atomic<ExecutionStatus> status_;
  };

} // namespace lazy


