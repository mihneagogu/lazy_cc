#pragma once

#include <deque>

#include "request.h"

namespace lazy {
	class ExecutionWorker {
		public:
			ExecutionWorker(const std::vector<Request*>& reqs) {
				q_ = std::deque<Request*>{reqs.begin(), reqs.end()};
			}

			void run();
			void execute();
		private:
			std::deque<Request*> q_;

	};

}
