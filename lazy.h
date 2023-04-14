#pragma once

#include <random>
#include <vector>

#include "engines/lazy/table.h"
#include "engines/lazy/lazy_engine.h"
#include "engines/lazy/request.h"

namespace lazy {

struct Writes {
  Writes(std::mt19937& gen);
  std::vector<int> ws_;
};

void run();
int mock_tx(Request* self, Table* tb, int w1, int w2, int w3);
void subst_fn();
void sticky_fn();

} // namespace lazy
