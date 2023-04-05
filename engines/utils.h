#pragma once

#include <iostream>

namespace utils {
  template<typename Col>
    void print(Col&& col) {
      if (col.empty()) {
        std::cout << "{}" << std::endl;
      }
      auto it = col.begin();
      std::cout << "{" << *it;
      it++;
      for (; it != col.end(); it++) {
        std::cout << ", " << *it;
      }
      std::cout << "}" << std::endl;
    }
}


