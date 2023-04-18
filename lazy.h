#pragma once

#include <random>
#include <vector>

#include "engines/lazy/entry.h"
#include "engines/lazy/lazy_engine.h"
#include "engines/lazy/request.h"

namespace lazy {

struct Writes {
  Writes(std::mt19937& gen);
  std::vector<int> ws_;
};

void run();
void subst_fn();
void sticky_fn();

} // namespace lazy
