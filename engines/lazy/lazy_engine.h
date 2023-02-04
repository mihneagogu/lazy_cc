#include <limits>
#include <vector>

namespace sitcky {
  constexpr int INT_STICKY = std::numeric_limits<int>::min();
}


namespace lazy {

  using Time = int;
  namespace constants {
    static constexpr Time T0 = 1;
    static constexpr Time T_STICKY = 0;
    static constexpr Time T_INVALID = -1;
    static constexpr int TIMESTAMPS_PER_TUPLE = 5;
  }

  struct IntSlot {
    IntSlot(Time t, int value): t_(t), val_(value) {}
    IntSlot(): t_(constants::T_INVALID) {}
    Time t_;
    int val_;
  };


  class IntColumn {
    public:
      IntColumn(int ntuples): ntuples_(ntuples) {
        data_.reserve(constants::TIMESTAMPS_PER_TUPLE * ntuples);
      }
      static IntColumn from_raw(int ntuples, int* data);
      IntColumn(std::vector<int> data);
    private:

      // What is the logical capacity of records of this
      int ntuples_;

      // How many latest-version records are we storing? (either sticky or not)
      int occupied_;

      // How many slots are occupied (a record with x written-to timestamps occupies x actual size slots)
      int actual_size_; 
      
      // Worth using manual allocation to avoid default ctor being called everywhere?
      std::vector<IntSlot> data_;
  };

  class DependencyGraph {

  };

  // columnar format?
  // array of [5; tuple.a] for first field
  // array of [5; tuple.b] for second field
  // array of [5; tuple.c] for second field

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
  in terms of code
  TODO: Actually implement that here
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
