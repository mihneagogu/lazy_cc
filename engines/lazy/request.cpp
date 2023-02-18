#include "request.h"
#include "table.h"
#include "lazy_engine.h"

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

  Request::Tid Request::request_cnt = 0;

  void Request::insert_sticky(int slot) {
    Globals::table_->insert_at(0, slot, IntSlot::sticky(epoch_, tid_));
  }

    void Request::set_request_time() {
        tid_ = ++request_cnt;
        epoch_ = Globals::clock_.advance();
    }

  void Request::stickify() {
    if (rw_known_in_advance_) {
      for (int slot : write_set_) {
        insert_sticky(slot);
      }
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
  }


    void Request::substantiate() {

    }
} // namespace lazy

