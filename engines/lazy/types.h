#pragma once

#include <variant>
#include <optional>
#include <cinttypes>
#include <limits>

namespace lazy {
    using Time = int;
    using Tid = int;

    namespace constants {
        static constexpr int64_t T0 = 1;
        static constexpr int64_t T_INVALID = std::numeric_limits<int>::max();
        static constexpr int TIMESTAMPS_PER_TUPLE = 5;
    } // namespace constants

    class CallingStatus {
        private:
            CallingStatus(): caller_(std::nullopt) {}
            std::optional<Tid> caller_;
        public:
            CallingStatus(Tid caller): caller_(caller) {}
            static CallingStatus client() {
                return CallingStatus();
            }
            bool is_tx() {
                return caller_.has_value();
            }
            bool is_client() {
                return !is_tx();
            }
            Tid get_tx() {
                return *caller_;
            }

    };
}
