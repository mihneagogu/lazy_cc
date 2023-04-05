#include <iostream>

#include "execution_worker.h"

namespace lazy {

	void ExecutionWorker::run() {
		while (!q_.empty()) {
			execute();
		}
	}

	void ExecutionWorker::execute() {
		if (!q_.empty()) {
			auto* head = q_.front();
			q_.pop_front();
			auto res = head->substantiate();
			if (res == SubstantiateResult::STALLED) {
				// std::cout << "stalled" << std::endl;
				// Request not stickified yet.
				// In a real system this should never be the case, since the 
				// substantiation threads only get requests after they have been stickified
				// and are ready to be executed.
				// Just put it in the back of the queue and start again
				q_.push_back(head);
			} else {
				// std::cout << "done" << std::endl;
			}
		}
	}
	
}
