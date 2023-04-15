#include "linked_table.h"
#include "logs.h"

#include <cassert>

namespace lazy {

LinkedIntColumn::LinkedIntColumn(std::vector<int>&& data) {
  data_ = std::vector<Bucket>();
  data_.reserve(data.size());
  for (std::vector<Bucket>::size_type i = 0; i < data.size(); i++) {
    // Only place where the move ctor should be called
    // since emplace requires that the type is move-ctible
    // in case of a reallocation due to a resize
    data_.emplace_back(Bucket(constants::T0, data[i]));
  }
}

LinkedIntColumn LinkedIntColumn::from_raw(int ntuples, int* data) {
  return LinkedIntColumn(std::vector<int>(data, data + ntuples));
}

void LinkedIntColumn::insert_at(int bucket, Time t, int val) {
    // Two txs are ordered wrt to the dependency graph if:
    // One reads the other's write to a slot. I.e the latter's readset
    // n former's writeset contains the given slot. 
    // However two txs which perform a blind write to a slot are not ordered
    // with respect to the timestamp ordering, therefore the insertions need to
    // be synchronised.
    data_[bucket].push(t, val);
}

LinkedTable::LinkedTable(std::vector<LinkedIntColumn>* cols): cols_(cols) {
    int tb_size = (*cols)[0].size();
    last_substantiations_ = std::vector<std::atomic<Time>>(tb_size);
    for (int i = 0; i < tb_size; i++) {
        last_substantiations_[i].store(constants::T0, std::memory_order_seq_cst);
    }
}

int LinkedIntColumn::size() const {
    return data_.size();
}

void LinkedTable::insert_at(int col, int bucket, Time t, int val) {
    (*cols_)[col].insert_at(bucket, t, val);
}

int LinkedTable::safe_read_int(int slot, int col, Time t, CallingStatus call) {
    if (t == constants::T0) {
        // For now we assume the table was created
        // at time constants::T0 and each slot has value 1, so if we receive
        // a request for a read at time consants::T0 it means it must be a 1,
        // and it wasn't performed by any transaction, but it comes from the initialisation
        return 1;
    }

    // Distinguish between a safe read being called by a client and one being 
    // called by the tx during computation itself. If it's a client call, 
    // check whether tx has finished. If it's still running, call 
    // substantiation and wait on it to finish. It's imoprtant that the 
    // decision is based on the caller, rather than the value of the entry, 
    // since one entry at time t could represent more than 1 write, 
    // and even if the entry is not a sticky, the tx could still be running 
    // (consider a tx which performs two writes. After the first write is 
    // performed the entry is not a sticky anymore, but the tx has not been 
    // fully substantiated either. Transactions are atomic, which means 
    // no intermetidate state should be leaked to the client.

    cout << "safe read int slot " << slot << " which was written at time " << t << endl;
    auto& column = (*cols_)[col].data_;
    auto responsible_tx = Globals::txs_.at(t);
    auto status = responsible_tx->execution_status();
    if (status == ExecutionStatus::DONE) {
        auto e = column[slot].entry_at(t);
        assert(e.has_value());
        cout << "read to " << slot << " at t " << t << " has value " << e->val_ << endl;
        return e->val_;
    };
    if (call.is_client()) {
        // either substantiate (or wait for substantiation to finish) and read the value afterwards
        cout << " client substantiating txid " <<  responsible_tx->tx_id() << endl;
        responsible_tx->substantiate();
        cout << responsible_tx->tx_id() << " done substantiating via client call" << endl;
    } else {
        cout << "read performed by tx " << call.get_tx() << " with time " << Globals::dep_.tx_of(call.get_tx())->time() << " on slot " << slot << " from time " << t << endl;
    }
    // At this point all the writes that this tx depends on
    auto e = column[slot].entry_at(t);
    assert(e.has_value());
    assert(!e->is_sticky());
    cout << "read to " << slot << " at t " << t << " has value " << e->val_ << endl;
    return e->val_;
}

void LinkedTable::safe_write_int(int slot, int col, int val, Time t) {
    // When this is called, it is assumed that all the reads that the write depends on
    // have been executed, as well as all other dependant transactions 
    // (since the reads must have happened earlier to get the value, in which case
    // they triggered a substification chain, or this was a blind write, in which
    // case this does not matter)
    
    cout << "safe write to slot " << slot << endl;
    auto& column = (*cols_)[col].data_;
    auto& bucket = column[slot];
    bucket.write_at(t, val);
}

int LinkedTable::rows() const {
    return (*cols_)[0].size();
}

int LinkedTable::checksum() {
    int nrows = rows();
    auto& col = (*cols_)[0];
    int sum = 0;
    for (int i = 0; i < nrows; i++) {
        cout << "slot " << i << " latest val computation " << endl;
        sum += col.data_[i].latest_value();
    }
    return sum;
}


void LinkedTable::enforce_wirte_set_substantiation(Time new_time, const std::vector<int>& write_set) {
  for (auto slot : write_set) {
    Time before = last_substantiations_[slot].load(std::memory_order_seq_cst);
    Time best_time = std::max(before, new_time);
    // Note, this is a best-effort approach. We try our best to put the max slot in, but it's possible that a third thread performs this operation before our load and cas, therefore making the cas fail. We could ensure the highest slot by doing a CAS loop, however. last_substantiations is used as a heuristic, nonetheless, so it seems unnecessary
    /* bool _succ = */ last_substantiations_[slot].compare_exchange_strong(before, best_time, std::memory_order_seq_cst);
  }

}

LinkedTable::~LinkedTable() {
  if (cols_) {
    delete cols_;
  }
}

}
