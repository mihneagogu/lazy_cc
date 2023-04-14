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

int LinkedTable::safe_read_int(int slot, int col, Time t) {
    if (t == constants::T0) {
        // TODO: remove hardcode. But for now we assume the table was created
        // at time constants::T0 and each slot has value 1, so if we receive
        // a request for a read at time consants::T0 it means it must be a 1,
        // and it wasn't performed by any transaction, but it comes from the initialisation
        return 1;
    }
    cout << "safe read int slot " << slot << " which was written at time " << t << endl;
    // ------------------------------
    // When a client requests a read then it finds the latest version of a value,
    // then gets the occupied counter, then tries to safe read the value at that counter.
    auto& column = (*cols_)[col].data_;
    int64_t val;
    Entry::EntryData entry;
    Time entry_t;
    bool found = false;

    auto& bucket = column[slot];
    auto& curr = bucket.head_;
    Bucket::BucketNode* e = nullptr;
    while ((e = curr.load(std::memory_order_seq_cst)) != nullptr) {
        entry = e->entry_.load(std::memory_order_seq_cst);
        cout << "entry for slot " << slot << " has time " << entry.t_ << endl;
        if (entry.has_time(t)) {
            found = true;
            val = entry.val_;
            entry_t = entry.t_;
            cout << "found desired entry at time " << entry.t_ << endl;
            break;
        }
        curr = e->next_.load(std::memory_order_seq_cst);
    }

    // If for some reason the value was not found (i.e some thread was asked to 
    // read it but the stickification thread has not even written the sticky) return
    // INT_MIN and log
    if (!found) {
      // TODO: log this somewhere (in an append-only log?)
      cout << "Entry " << slot << " not found for time " << t << " | "
          << " slot has " << Globals::table_->size_at(slot, 0) << " entries " << endl;
      return std::numeric_limits<int>::min();
    }

    // The entry might not be a sticky but last_subst < t
    // Some thread might have written to it but not updated last_substantiations
    // or two threads with different timestamp wrote to last_substantiations
    // but the one with lower time won.
    if (!entry.is_sticky()) {
        // Nobody will ever write this slot anymore, so just 
        // find the value
        return val;
    }

    cout << "sticky entry has time " << entry_t << endl;
    assert(entry_t < 0); // This must be a sticky! 
    // Nobody has substantiated this sticky, so let's do it ourselves.
    Tid tx_id = val;
    auto* tx = Globals::dep_.tx_of(tx_id);
    auto status = tx->execution_status();
    if (status != ExecutionStatus::EXECUTING_NOW) {
        // Go on if we haven't started substantiating the transaction.
        // If the tx is being executed, it means that the transaction has more
        // safe_read_int() calls and we are the 2nd (or bigger) call.
        cout << "substantiating txid " <<  tx->tx_id() << endl;
        tx->substantiate();
        cout << "done substantiating txid " <<  tx->tx_id() << endl;
    } else {
        cout << "avoiding re-entrant call to substantiate() for tx " << tx->tx_id() << endl;
    }

    // TODO: Keep track of the pointer that previously held the entry
    // and re-load the data itself? The entry should be written to inplace
    // so there should be no need to retraverse the list
    auto& curr_ = column[slot].head_;
    while ((e = curr_.load(std::memory_order_seq_cst)) != nullptr) {
        entry = e->entry_.load(std::memory_order_seq_cst);
        if (entry.has_time(t)) {
            return entry.val_;
        }
        curr_ = e->next_.load(std::memory_order_seq_cst);
    }
    throw std::runtime_error("unreachable");
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
