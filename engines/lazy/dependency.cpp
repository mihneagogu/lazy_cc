#include "dependency.h"

namespace lazy {

void DependencyGraph::add_dep(Tid tx, Tid on) {
  dependencies_[tx].push_back(on);
}

} // namespace lazy
