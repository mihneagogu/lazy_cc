#include "request.h"
#include "entry.h"
#include "linked_table.h"
#include "tx_coordinator.h"
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
      epoch_ = Globals::clock_.advance();
      tid_ = epoch_;
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
      // cout << "tx " << tid_ << " reads " << write1_ << " from write performed at " << read1_t_ << endl;
      insert_sticky(write1_);
      Globals::dep_.sticky_written(tid_, write1_);

      read2_t_ = Globals::dep_.time_of_last_write_to(write2_);
      // cout << "tx " << tid_ << " reads " << write2_ << " from write performed at " << read2_t_ << endl;
      insert_sticky(write2_);
      Globals::dep_.sticky_written(tid_, write2_);

      read3_t_ = Globals::dep_.time_of_last_write_to(write3_);
      // cout << "tx " << tid_ << " reads " << write3_ << " from write performed at " << read3_t_ << endl;
      insert_sticky(write3_);
      Globals::dep_.sticky_written(tid_, write3_);

      Globals::coord_->confirm_stickify(tid_);
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
    Globals::coord_->confirm_stickify(tid_);
  }

  SubstantiateResult Request::substantiate() {
    // cout << "substantiating this request with txid " << tx_id() << endl;
    auto status = Globals::coord_->info_at(epoch_).status();
		if (status == ExecutionStatus::UNINIT) {
      throw std::runtime_error("UNINIT ENTRY");
			return SubstantiateResult::STALLED;
		}
    if (status == ExecutionStatus::DONE) {
      // Someone else already executed this transaction!
      return SubstantiateResult::SUCCESS;
    }
    // TODO: Add a executing_now check here too to avoid needlessly acquiring the lock
    
    // SUG: Use trylock and do something useful if someone is executing this?
    std::scoped_lock<std::mutex> execute(Globals::coord_->info_at(epoch_).tx_lock_);
    status = Globals::coord_->info_at(epoch_).status();

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

    // Must be a sticky then! Substantiate all the transactions that this 
    // tx depends on
    auto deps = Globals::dep_.get_dependencies(tid_);
    for (auto tx : deps) {
      auto _res = Globals::coord_->tx_at(tx).substantiate();
			// The result here should never be stalled or failed,
			// since the sticky thread itself made the dependency graph
    }
    
    // We are the only thread which can perform the computation. Do it now
    auto& info = Globals::coord_->info_at(epoch_);

    info.status_.store(ExecutionStatus::EXECUTING_NOW, std::memory_order_seq_cst);
    // cout << "calling fp!" << endl; 
    fp_(this, Globals::table_, write1_, write2_, write3_);

    info.status_.store(ExecutionStatus::DONE, std::memory_order_seq_cst);
		return SubstantiateResult::SUCCESS;
  }

} // namespace lazy

