/**
 * ThreadPool class
 *
 * @brief Fixed-size worker pool executing queued tasks.
 * @date 14-07-2026
 */

#include "server/thread_pool.h"

ThreadPool::ThreadPool(size_t numThreads) : stopping(false) {
  for (size_t i = 0; i < numThreads; ++i) {
    workers.emplace_back([this] { workerLoop(); });
  }
}

ThreadPool::~ThreadPool() { shutdown(); }

void ThreadPool::task(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (stopping) {
      return;
    }
    tasks.push(std::move(task));
  }
  condition.notify_one();
}

void ThreadPool::shutdown() {
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (stopping) {
      return;
    }
    stopping = true;
  }
  condition.notify_all();
  for (auto &worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  workers.clear();
}

void ThreadPool::workerLoop() {
  while (true) {
    std::function<void()> job;
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      condition.wait(lock, [this] { return stopping || !tasks.empty(); });
      if (stopping && tasks.empty()) {
        return;
      }
      job = std::move(tasks.front());
      tasks.pop();
    }
    job();
  }
}
