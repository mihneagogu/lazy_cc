#pragma once

#include <vector>
#include <atomic>

#include "request.h"

namespace lazy {

  struct RequestInfo {
      std::mutex tx_lock_; // TODO: Use a better notification mechanism than a lock? Condvar? Publishing board?
      Request req_;
      std::atomic<ExecutionStatus> status_;

      RequestInfo(const Request& req): req_(req), status_(ExecutionStatus::UNINIT) {}
      RequestInfo(RequestInfo&& other): req_(std::move(other.req_)) {
        // TO NEVER BE USED. This is required just at initialisation time 
        // because std::vector<> requires move ctor for realloc
        status_.store(other.status());
      }

      ExecutionStatus status() const;
      bool is_being_executed() const;
      bool was_performed() const;
      bool is_sticky() const;
  };

  class TxCoordinator {
    public:
      TxCoordinator() = default;
      TxCoordinator(const std::vector<Request>& reqs) {
        txs_.reserve(reqs.size());
        for (const auto& r : reqs) {
          txs_.emplace_back(r);
        }
      }
      RequestInfo& info_at(Time t);
      Request& tx_at(Time T);
      void confirm_stickify(Tid tid);

    private:
      std::vector<RequestInfo> txs_;

  };

}
