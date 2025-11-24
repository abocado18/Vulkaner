#pragma once
#include <deque>
#include <functional>

template <typename... Ts> struct DeletionQueue {
  std::deque<std::function<void(Ts *...)>> deletors;

  void pushFunction(std::function<void(Ts *...)> &&f) { deletors.push_back(f); }

  void flush(Ts *...ts) {
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
      (*it)(ts...);
    }

    deletors.clear();
  }
};