

/*
  8/10 cores of machine used
  each of the 8 threads is worker thread 
*/

/*
  For 2pc transaction: look at the operations, then, see all the locks that you need
  to own, then execute the code when you have them

  How it works: 
  Acquire a lock every time you want access data as part of a transaction. 
  Before locking, need to check whether you are creating a deadlock. If yes, abort
  and get state back to previous time, by saving in memory previous state 
  of the record and reverting if necessary.

  Is there any way of running into a bug where you revert over someone else's write?
  Clearly not, since you only drop locks at the end of the transaction, which means
  you own the lock for that record, which means that nobody else could have written to it
  after yourself
  
*/


