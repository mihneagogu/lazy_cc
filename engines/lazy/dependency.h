#pragma once

#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>

#include "request.h"
#include "types.h"

namespace lazy {

class Request;

class DependencyGraph {
  public:
    void add_dep(Tid tx, Tid on);
  private:
    std::unordered_map<Tid, std::vector<Tid>> dependencies_;
    std::unordered_map<Tid, Request*> txs_;
};

} // namespace lazy
