#pragma once

#include <limits>
#include <vector>


namespace lazy {

  using Time = int;
  namespace constants {
    static constexpr Time T0 = 1;
    static constexpr Time T_STICKY = 0;
    static constexpr Time T_INVALID = -1;
    static constexpr int TIMESTAMPS_PER_TUPLE = 5;
  }


  class Clock {
    public:
      Clock(): current_time_(constants::T0) {}
      int time() const;
      void advance();
    private:
      // SUG: Use something which doesn't hammer this variable in a concurrent
      // context?
      int current_time_; 
  };

  static Clock clock;

  class Globals {
    public:
      // SUG: folly::Singleton ?
      static Clock clock;
  };

  class DependencyGraph {

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

*/


} // namespace lazy
