#include "request.h"
#include "table.h"
#include "linked_table.h"
#include "lazy_engine.h"
#include "logs.h"

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
    Globals::table_->insert_at(0, slot, IntSlot::sticky(epoch_, tid_));
    Globals::dep_.sticky_written(tid_, slot);
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
      for (int slot : write_set_) {
        insert_sticky(slot);
      }
      auto _ = Globals::dep_.get_dependencies(tid_);
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
    cout << "this " << this << endl;
		if (!stickified_.load(std::memory_order_seq_cst)) {
			return SubstantiateResult::STALLED;
		}
    if (was_performed()) {
      // Someone else already executed this transaction!
      return SubstantiateResult::SUCCESS;
    }
    // SUG: Use trylock and do something useful if someone is executing this?
    std::scoped_lock<std::mutex> execute(tx_lock_);
    if (was_performed()) {
      // Maybe someone else executed it while we were trying to acquire the lock
      // in which case we don't need to reexecute the code
      return SubstantiateResult::SUCCESS;
    }

    // Substantiate all the transactions that this trans depends on
    auto deps = Globals::dep_.get_dependencies(tid_);
    for (auto* tx : deps) {
      auto _res = tx->substantiate();
			// The result here should never be stalled or failed,
			// since the sticky thread itself made the dependency graph
    }
    
    // We are the only thread which can perform the computation. Do it now
    fp_(this, Globals::table_, write1_, write2_, write3_);

    computation_performed_.store(true, std::memory_order_seq_cst);
    Globals::table_->enforce_wirte_set_substantiation(epoch_, write_set_);
		return SubstantiateResult::STALLED;
  }

  bool Request::was_performed() const {
    // SUG: Should be able to use relaxed here, or at least
    // release?
    return computation_performed_.load(std::memory_order_seq_cst);
  }

} // namespace lazy

