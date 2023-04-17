#include "request.h"
#include "table.h"
#include "linked_table.h"
#include "lazy_engine.h"
#include "logs.h"
#include "../utils.h"

using std::memory_order;

namespace lazy {

  bool Operation::is_read() const {
    return ty_ == OperationTy::READ;
  }

  bool Operation::is_write() const {
    return ty_ == OperationTy::WRITE;
  }

  int Operation::read_slot() const {
    return op_.read_.slot_;
  }

  int Operation::write_slot() const {
    return op_.write_.slot_;
  }

  Tid Request::request_cnt = 0;

  void Request::insert_sticky(int slot) {
    if (Globals::dep_.time_of_last_write_to(slot) == epoch_) {
      // If we have already written a sticky to this slot for our epoch
      // don't write another entry in the slot's bucket
      return;
    }
    Globals::table_->insert_at(0, slot, -epoch_, tid_);
  }

  void Request::set_request_time() {
      tid_ = ++request_cnt;
      epoch_ = Globals::clock_.advance();
  }

  Time Request::time() const {
    return epoch_;
  }

  Tid Request::tx_id() const {
    return tid_;
  }

  void Request::stickify() {
    if (rw_known_in_advance_) {
      Globals::dep_.check_dependencies(tid_, read_set_);
      
      // TODO: add the read times to the vector<Operation> rather than hardcoded
      // for (int slot : write_set_) {
        // insert_sticky(slot);
        // Globals::dep_.sticky_written(tid_, slot);
      // }

      read1_t_ = Globals::dep_.time_of_last_write_to(write1_);
      cout << "tx " << tid_ << " reads " << write1_ << " from write performed at " << read1_t_ << endl;
      insert_sticky(write1_);
      Globals::dep_.sticky_written(tid_, write1_);

      read2_t_ = Globals::dep_.time_of_last_write_to(write2_);
      cout << "tx " << tid_ << " reads " << write2_ << " from write performed at " << read2_t_ << endl;
      insert_sticky(write2_);
      Globals::dep_.sticky_written(tid_, write2_);

      read3_t_ = Globals::dep_.time_of_last_write_to(write3_);
      cout << "tx " << tid_ << " reads " << write3_ << " from write performed at " << read3_t_ << endl;
      insert_sticky(write3_);
      Globals::dep_.sticky_written(tid_, write3_);

      stickified_.store(true, std::memory_order_seq_cst);
      return;
    }

    // If rw sets are not known in advance compute 
    // the required slots via interpreting the pseudo-instructions
    for (const auto& op : operations_) {
      if (op.is_read()) {
        read_set_.push_back(op.read_slot());
      } else if (op.is_write()) {
        auto slot = op.write_slot();
        write_set_.push_back(slot);
        insert_sticky(slot);
      }
    }
    Globals::dep_.check_dependencies(tid_, read_set_);
    stickified_.store(true, std::memory_order_seq_cst);
  }

  SubstantiateResult Request::substantiate() {
    cout << "substantiating this request with txid " << tx_id() << endl;
		if (!stickified_.load(std::memory_order_seq_cst)) {
			return SubstantiateResult::STALLED;
		}
    if (was_performed()) {
      // Someone else already executed this transaction!
      return SubstantiateResult::SUCCESS;
    }
    
    // SUG: Use trylock and do something useful if someone is executing this?
    std::scoped_lock<std::mutex> execute(tx_lock_);
    auto status = execution_status();
    if (status == ExecutionStatus::DONE) {
      // Maybe someone else executed it while we were trying to acquire the lock
      // in which case we don't need to reexecute the code
      return SubstantiateResult::SUCCESS;
    }
    if (status == ExecutionStatus::EXECUTING_NOW) {
      // We are already trying to execute the computation right now
      // so abort, since we do not want any re-entrancy problems
      return SubstantiateResult::RUNNING;
    }

    // Substantiate all the transactions that this trans depends on
    auto deps = Globals::dep_.get_dependencies(tid_);
    for (auto* tx : deps) {
      auto _res = tx->substantiate();
			// The result here should never be stalled or failed,
			// since the sticky thread itself made the dependency graph
    }
    
    // We are the only thread which can perform the computation. Do it now
    status_.store(ExecutionStatus::EXECUTING_NOW, std::memory_order_seq_cst);
    cout << "calling fp!" << endl; 
    fp_(this, Globals::table_, write1_, write2_, write3_);

    Globals::table_->enforce_wirte_set_substantiation(epoch_, write_set_);
    status_.store(ExecutionStatus::DONE, std::memory_order_seq_cst);
		return SubstantiateResult::SUCCESS;
  }

  ExecutionStatus Request::execution_status() const {
    return status_.load(std::memory_order_seq_cst);
  }

  bool Request::was_performed() const {
    return execution_status() == ExecutionStatus::DONE;
  }

  bool Request::is_being_executed() const {
    return execution_status() == ExecutionStatus::EXECUTING_NOW;
  } 

} // namespace lazy

