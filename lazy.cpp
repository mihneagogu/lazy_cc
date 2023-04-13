#include <iostream>
#include <vector>
#include <thread>
#include <random>
#include <memory>
#include <utility>
#include <atomic>

#include "lazy.h"
#include "engines/lazy/execution_worker.h"
#include "engines/lazy/linked_table.h"

using std::cout;
using std::endl;

namespace lazy {

// TODO: For the substantiate function, actually move part of the code
// into ExecutorWorker class, so that it can keep track of the queue of requests
// to serve, and have it work as a circular queue in the case that it was asked
// to substantiate a request which has not yet been stickified.

void sticky_fn(std::vector<Request*>& reqs) {
  for (auto* req : reqs) {
    req->stickify();
  }
  cout << "stickification performed" << endl;
}

void client_calls(const std::vector<std::pair<int, int>>& writes) {
  std::random_device rd;  
  std::mt19937 gen(rd());  

  std::uniform_int_distribution<int> dis(0, writes.size() - 1);
  auto idx = dis(gen);

  auto slot = writes[idx].first;
  auto time = writes[idx].second;
  cout << "trying to read slot " << slot << " at time " << time  << endl;
  auto res = Globals::table_->safe_read_int(slot, 0, time);
}

int mock_computation(Request* self, LinkedTable* tb, int w1, int w2, int w3) {
  Time tx_t = self->time();
  Tid tid = self->tx_id();

  int r1 = tb->safe_read_int(w1, 0, self->read1_t_);
  cout << "Read r1 " << r1 << endl;
  cout << "Write w1 " << w1 << " at time " << tx_t << endl;
  tb->safe_write_int(w1, 0, r1 + 1, tx_t);
  int r2 = tb->safe_read_int(w2, 0, self->read2_t_);
  tb->safe_write_int(w2, 0, r2 + 1, tx_t);
  int r3 = tb->safe_read_int(w3, 0, self->read3_t_);
  tb->safe_write_int(w3, 0, r3 + 1, tx_t);

  return 3; // 3 writes
}

Request* mock_tx(std::mt19937& gen, std::vector<std::pair<int, int>>& writes) {
  // each transaction writes 3 ints
  std::uniform_int_distribution<int> dis(1, Globals::n_slots - 1);  // define the distribution
  int w1 = dis(gen);
  int w2 = dis(gen);
  int w3 = dis(gen);
  std::vector<int> ws{w1, w2, w3};
  std::vector<int> rs = ws;
  auto* req = new Request(true, mock_computation, {}, std::move(ws), std::move(rs));

  writes.emplace_back(w1, req->time());
  writes.emplace_back(w2, req->time());
  writes.emplace_back(w3, req->time());

  cout << "req " << req->tx_id() << " w1 w2 w3 ";
  cout << w1 << " " << w2 << " " << w3 << endl;
  req->set_write_to(w1, w2, w3);
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

  std::vector<int> tasks;
  std::vector<std::vector<Request*>> txs(4);
  std::vector<Request*> to_stickify;
  std::vector<std::pair<int, int>> writes;
  to_stickify.reserve(Globals::tx_count);

  int cores = Globals::subst_cores;
  for (int i = 0; i < Globals::subst_cores; i++) {
    cout << Globals::tx_count / cores << " for each core " << endl;
    txs[i].reserve(Globals::tx_count / cores);
    for (int j = 0; j < Globals::tx_count / cores; j++) {
      auto* req = mock_tx(gen, writes);
      txs[i].emplace_back(req);
      to_stickify.emplace_back(req);
    }
  }
  Globals::dep_.add_txs(to_stickify);
  
  std::vector<int> data(Globals::n_slots, 1);
  auto* cols = new std::vector<LinkedIntColumn>();
  cols->emplace_back(std::move(data));
  Globals::table_ = new LinkedTable(cols);

  std::vector<std::thread> ts;
  sticky_fn(to_stickify);
  
  for (int i = 0; i < cores; i++) {
    ts.emplace_back(client_calls, std::ref(writes));
  }
  for (auto& t : ts) {
    t.join();
  }

  lazy::Globals::shutdown();
  for (auto* req : to_stickify) {
    delete req;
  }

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
