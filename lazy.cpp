#include <iostream>

#include "lazy.h"

namespace lazy {

void run() {
  std::cout << "Hello Lazy BOSS!" << std::endl;
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
