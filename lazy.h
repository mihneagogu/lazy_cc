#pragma once

#include "engines/lazy/table.h"
#include "engines/lazy/lazy_engine.h"
#include "engines/lazy/request.h"

namespace lazy {

void run();
int mock_tx(Request* self, Table* tb, int w1, int w2, int w3);
void subst_fn();
void sticky_fn();

} // namespace lazy
