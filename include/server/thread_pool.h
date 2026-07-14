/**
 * ThreadPool header file class
 *
 * @date 14-07-2026
 */
#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
  // constructor
  ThreadPool(size_t numThreads);

  // destructor
  ~ThreadPool();

  // enqueue a task
  void task(std::function<void()> task);

  // shutdown the thread pool
  void shutdown();

private:
  // workers threads
  std::vector<std::thread> workers;

  // tasks queue
  std::queue<std::function<void()>> tasks;

  // queue mutex
  std::mutex queueMutex;

  // condition variable
  std::condition_variable condition;

  // is the thread pool stopping
  bool stopping;

  // worker loop
  void workerLoop();
};