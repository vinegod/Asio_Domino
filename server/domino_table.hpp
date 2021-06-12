#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <set>
#include <string>
#include <utility>

class dominoes {
public:
  dominoes () {
    srand(1);
  }
  
  std::string get_dominoes_str(int count) {
    
    char buffer[168];
    for (int i = 0; i < count; i++) {
      int k = rand() % elements_.size();
      auto it = elements_.begin();
      std::advance(it, k);
      sprintf(buffer, "%s [%d|%d]", buffer, it->first, it->second);
      elements_.erase(it);
    }
    return std::string(buffer);
  }
  auto get_domino() {
    
    int k = rand() % elements_.size();
    auto it = elements_.begin();
    std::advance(it, k);
    auto element = *it;
    elements_.erase(it);
    return element;
  }

  std::size_t size() const { return elements_.size(); }
  bool empty() const { return elements_.empty(); }
  auto &data() { return elements_; }

private:
  std::set<std::pair<int, int>> elements_{
      {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6},
      {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {2, 2},
      {2, 3}, {2, 4}, {2, 5}, {2, 6}, {3, 3}, {3, 4}, {3, 5},
      {3, 6}, {4, 4}, {4, 5}, {4, 6}, {5, 5}, {5, 6}, {6, 6}};
      
};