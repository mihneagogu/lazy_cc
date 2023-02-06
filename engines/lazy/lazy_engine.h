#pragma once

#include <limits>
#include <vector>

#include "types.h"


namespace lazy {

  class Table;

  namespace constants {
    static constexpr Time T0 = 1;
    static constexpr Time T_INVALID = std::numeric_limits<int>::max();
    static constexpr int TIMESTAMPS_PER_TUPLE = 5;
  } // namespace constants


  class Clock {
    public:
      Clock(): current_time_(constants::T0) {}
      Time time() const;
      Time advance();
    private:
      // SUG: Use something which doesn't hammer this variable in a concurrent
      // context?
      int current_time_; 
  };

  static Clock clock;

  class Globals {
    public:
      // SUG: folly::Singleton ?
      static Clock clock_;
      static Table* table_;
      static void shutdown();
  };

  /*
     8/10 cores used. Why 8/10 only though? (perhaps assuming other work is used on the other 2 cores, e.g os, other infrastructure which is running on the machine?)
     Single-threaded stickification layer
     multi-threaded substantiation layer
     */


  /*
Substantiation: multiple threads running in parallel
the stickification layer ensures every transaction maintains a reference to its dependencies
worker thread looks if dependency has been evaluated.
If yes, proceed, it should be safe to evaluate
(Q: What happens if a transaction that depends on  T adds T to its dependencies?)
(Q: Say A -> B and A -> C. C and B want to execute at the same time, but both depend on A.
Do we lock parts of the dependency graph then, and make the other transaction just wait?
Each transaction data is augmented by spinlock bit)
(Q for future: They claim that this has better cache affinity, but realistically only if all
of the transactions' data cummulated fit into cache, is there any way of trying to 
do this by aggregating computation across >1 transaction, basically interleaving transaction
logic if this is sound? Maybe another thread which analyzes transaction structure
while it's not being substantiated)
*/

/*
  TODO: Exemplify lazy transaction layout in memory and how it works exactly
  in terms of code (X)
  TODO: Create dependency graph when stickifying
  TODO: Ask about SQL interpretation, how we should do it, how it's usually done?

   To actually store an array of transcations probably need type erasure 
    or clasic c-style function pointers
  void transaction1(Database& db) {
    // this is actual execution
    db->tuple_a.x = db->tuple_s.b * db->tuple_z.a;
    db->tuple_b.x = 2;
    db->tuple_c.y = 3;
    db->tuple_d.z = 4;
  }

  After sitckification phase we should store something like this, so perhaps
  besides the code itself there should be an interpreted version of the code which
  contains the commands [Write(...) = Read(...) * Read(...), Write(...) = 2]
  class Trans {
    function_pointer;

    to be determined while looking at transaction code or by partially running
    it
    read_set; 

    to be determined while looking at transaction code or by partially running
    it
    write_set; <- to be determined while looking at transaction code
  }

  Generally, transactions are split between now and later phases:
  Split is done manually by the user (assumed here).
  The now-phase contains at least all of the reads needed to do all the integrity checks (and potentially actually
  all the computation), after which stickies are inserted.
  Stickification itself is only part of the now-phase (at the end), potentially the now-phase could
  include more than stickification (according to the paper). Sometimes reading of records
  is inevitable if the system has no way of telling where to write the stickies 
  when reading the query. The now-phase *must* contain all the work necessary to
  determine a transaction's read/write-set

  The dependency graph cannot be done when stickification happens. A transactioni
  is dependent on another when it tries to read a record which is a sticky made 
  by another transaction. Since we do not do any record reads (hopefully, realistically we do for IC)
  when putting stickies, this has to happend dynamically at substantiation time.

  For this implementation, the now-phase will only contain stickification.

  Substantiation happens when a client attempts to read a sticky, in which case
  the transaction which made that sticky has to run and the dependency graph is built.

  stickify(): 
  - construct read/write set of each transaction based on the operations
  - insert sticky for write operation

  when a client tries to read a value and there is a sticky, that's when we
  substantiate():
  - execute transaction which caused read, and all the transactions it transitively
  depends on (locking the graph on that path)

  Realistically in the now-phase, when the implementation is more complete, we need
  to do more things, like integrity checking, constraint violation and other stuff

*/


} // namespace lazy
