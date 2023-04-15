#include "tx_collection.h"

namespace lazy {

  Request* TxCollection::at(Time t) {
    if (t == constants::T0) {
      return nullptr;
    }
    return txs_[t - constants::T0 - 1];
  }

}
