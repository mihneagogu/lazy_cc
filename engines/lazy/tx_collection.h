#pragma once

#include <vector>

#include "types.h"

namespace lazy {
  
  class Request;
  class TxCollection {
    public:
      TxCollection() = default;
      TxCollection(std::vector<Request*> txs): txs_(std::move(txs)) {}
      Request* at(Time t);
    private:
      std::vector<Request*> txs_;
  };

}
