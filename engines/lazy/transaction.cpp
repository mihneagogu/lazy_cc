#include "transaction.h"
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

  void Transaction::stickify() {
    for (const auto& op : operations_) {
      if (op.is_read()) {
        read_set_.push_back(op.read_slot());
      } else if (op.is_write()) {
        // TODO: actually insert sticky? But at what time?
        write_set_.push_back(op.write_slot());
      }

      // TODO: dependency graph creation
    }
    auto t = Globals::clock.time();
  }
} // namespace lazy

