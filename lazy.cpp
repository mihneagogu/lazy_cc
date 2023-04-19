#include <iostream>
#include <set>
#include <vector>
#include <thread>
#include <random>
#include <memory>
#include <utility>
#include <atomic>
#include <algorithm>
#include <random>
#include <chrono>

#include "lazy.h"
#include "engines/lazy/execution_worker.h"
#include "engines/lazy/linked_table.h"
#include "engines/lazy/tx_coordinator.h"

using std::cout;
using std::endl;

namespace lazy {

void sticky_fn(Time last_tx) {
  for (Time t = constants::T0 + 1; t <= last_tx; t++) {
    Globals::coord_->tx_at(t).stickify();
  }
  cout << "stickification performed" << endl;
}

void client_calls(const std::vector<std::pair<int, int>>& writes) {
  std::vector<std::pair<int, int>> accesses = writes;
  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

  shuffle (accesses.begin(), accesses.end(), std::default_random_engine(seed));
  for (const auto& access : accesses) {
    Globals::table_->safe_read_int(access.first, 0, access.second, CallingStatus::client());
  }
}

int mock_computation(Request* self, LinkedTable* tb, int w1, int w2, int w3) {
  Time tx_t = self->time();
  auto tx_call = CallingStatus(self->tx_id());

  int r1 = tb->safe_read_int(w1, 0, self->read1_t_, tx_call);
  tb->safe_write_int(w1, 0, r1 + 1, tx_t);
  int r2 = tb->safe_read_int(w2, 0, self->read2_t_, tx_call);
  tb->safe_write_int(w2, 0, r2 + 1, tx_t);
  int r3 = tb->safe_read_int(w3, 0, self->read3_t_, tx_call);
  tb->safe_write_int(w3, 0, r3 + 1, tx_t);

  return 3; // 3 writes
}

Writes::Writes(std::mt19937& gen) {
  // each transaction writes 3 ints
  // For now, only allow 3 different writes in each transaction
  // as to avoid two stickies being written to the same slot in the same epoch
  // TODO: fix this problem

  std::uniform_int_distribution<int> dis(1, Globals::n_slots - 1);  // define the distribution
  ws_.push_back(dis(gen));
  ws_.push_back(dis(gen));
  ws_.push_back(dis(gen));
}

Request mock_tx(std::mt19937& gen, std::vector<std::pair<int, int>>& writes) {
  Writes w(gen);
  int w1 = w.ws_[0];
  int w2 = w.ws_[1];
  int w3 = w.ws_[2];


  std::vector<int> ws{w.ws_.begin(), w.ws_.end()};
  std::vector<int> rs = ws;
  auto req = Request(true, mock_computation, {}, std::move(ws), std::move(rs));

  writes.emplace_back(w1, req.time());
  writes.emplace_back(w2, req.time());
  writes.emplace_back(w3, req.time());

  req.set_write_to(w1, w2, w3);
  return req;
}


void run() {
  if (!std::atomic<Entry::EntryData>().is_lock_free()) {
    cout << "Entry data is not lock free. Aborting!" << endl;
    exit(1);
  }

  std::random_device rd;  
  std::mt19937 gen(rd());  

  // 100M slots of ints, so 500M ints, which is 2GB,
  // and assuming each slot has 2 ints (1 for the value 1 for the slot)
  // this is around 4GB of memory occupied by the table

  int cores = Globals::subst_cores;
  std::vector<std::vector<Tid>> txs(cores);
  std::vector<Request> to_stickify;
  std::vector<std::pair<int, int>> writes;
  to_stickify.reserve(Globals::tx_count);


  for (int i = 0; i < Globals::subst_cores; i++) {
    txs[i].reserve(Globals::tx_count / cores);
    for (int j = 0; j < Globals::tx_count / cores; j++) {
      auto req = mock_tx(gen, writes);
      txs[i].emplace_back(req.tx_id());
      to_stickify.emplace_back(req);
    }
  }
  
  std::vector<int> data(Globals::n_slots, 1);
  auto* cols = new std::vector<LinkedIntColumn>();
  cols->emplace_back(std::move(data));
  Globals::table_ = new LinkedTable(cols);
  Globals::coord_ = new TxCoordinator(to_stickify);

  std::vector<std::thread> ts;
  sticky_fn(Globals::tx_count + constants::T0);
  
  for (int i = 0; i < cores; i++) {
    ts.emplace_back(client_calls, std::ref(writes));
  }
  for (auto& t : ts) {
    t.join();
  }

  cout << "checksum at the end: " << Globals::table_->checksum() << endl;

  lazy::Globals::shutdown();

  /* TODO:
    On main process requests, give them to the thread which does substantiation layer,
    and whenever a request which is not a transaction and involves a read, and the read
    is a sticky, then pass it off of to the substantiation layer which does the substantiation.

    Is the read unsychronized? well, the read needs to go through the bucket to search
    for the latest value, but potentially the stkicifcation layer is writing to it.
    So, in the strict sense, yes it is unsychronized. How about a relaxed read of
    the value?

    The sticky thread always writes stickies to the columns, and whenever a 
    transaction needs to be substantiated, it depends on another one if it needs to read one of the stickies
    of the other transaction. Can the value that the substantiated thread reads be changed 
    while it's looking at it? A request may try to read the value, no problem.
    Scenario 1:
    If we need record1.x = record2.y + record3.z; And there is a sticky for record1.x,
    but record2.y and record3.z are already substantiated at the time of the transaction,
    then we need to read the values and write to record1. However, another request to 
    record1 can cause it to be read, which means the write must be atomic.
    After a record is substantiated, all reads can be unsychronized, since there will
    be no further write to that record at that time.

    Scenario 2:
    So the normal reads: need to be synchronized (atomic read)
    The substantiated writes: need to be synchronized (atomic write)
    What if in the example above, record2.y and record3.z are sitckies.
    In that case, another record could try to read them at the same time, so we need
    the access to be synchronized, so atomic reads are necessary.

    Scenario 3:
    Similarly to scenario 2, imagine that the write is a sticky (since we are substantiating)
    and the two read values are stickies too. In that case, is there a possibility of
    another substantiating thread to read the values at the same time? Well, it might 
    check to see if the value is a sticky, and if it is a sticky, it will try to execute
    the transaction which issued the sticky, but at that point it would have to 
    lock, which would fail. 

    Conclusion: any operations that are done to check whether a record is a sticky
    or the write of a substantiation must be synchronized, using an atomic read/write.

    When stickifying, do we need the write to be synchronized? 2 tactics:
    We could either have a counter which says how many versions we have active,
    and then write past it, then bump it atomically. Any thread which would read
    the counter would read data before it, so our write is safe. But the increment needs
    to be stronger than relaxed, so that the effect of the write which was before
    is visible (possibly release). Then any read of the counter should be aquire.

    If we allow dynamic versioning, then there needs to be an allocation pollicy,
    which means that substantiated values may be replaced, so the reads are not allowed
    to be synchronized.
    
  */
}

} // namespace lazy
