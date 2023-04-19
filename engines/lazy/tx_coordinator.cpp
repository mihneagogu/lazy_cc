#include "tx_coordinator.h"

namespace lazy {

  ExecutionStatus RequestInfo::status() const {
    return status_.load(std::memory_order_seq_cst);
  }

  bool RequestInfo::is_being_executed() const {
    return status() == ExecutionStatus::EXECUTING_NOW;
  }

  bool RequestInfo::was_performed() const {
    return status() == ExecutionStatus::DONE;
  }

  bool RequestInfo::is_sticky() const {
    return status() == ExecutionStatus::STICKY;
  }

  RequestInfo& TxCoordinator::info_at(Time t) {
    return txs_[t - constants::T0 - 1];
  }

  Request& TxCoordinator::tx_at(Time t) {
    return info_at(t).req_;
  }

  void TxCoordinator::confirm_stickify(Tid t) {
    info_at(t).status_.store(ExecutionStatus::STICKY, std::memory_order_seq_cst);
  }


}


