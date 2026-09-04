/**
 * ThreadPool class
 *
 * @brief Fixed-size worker pool executing queued tasks.
 * @date 03-09-2026
 */

#include "server/thread_pool.h"

// constructor
ThreadPool::ThreadPool(size_t numThreads) {
  // set stopping flag to false
  stopping = false;

  // create worker threads
  for (size_t i = 0; i < numThreads; ++i) {
    workers.emplace_back([this] { workerLoop(); });
  }
}

// destructor
ThreadPool::~ThreadPool() { shutdown(); }

// enqueue a task
void ThreadPool::task(std::function<void()> task) {
  {
    // check if the thread pool is stopping
    std::lock_guard<std::mutex> lock(queueMutex);
    if (stopping) {
      return;
    }

    // push the task to the queue
    tasks.push(std::move(task));
  }

  // notify one of the threads
  condition.notify_one();
}

// shutdown the thread pool
void ThreadPool::shutdown() {
  {
    // check if the thread pool is stopping
    std::lock_guard<std::mutex> lock(queueMutex);
    if (stopping) {
      return;
    }

    // set stopping flag to true
    stopping = true;
  }

  // wake all threads and join them if can
  condition.notify_all();
  for (auto &worker : workers) {
    if (worker.joinable()) {
      // #TODO: hard shutdown the thread
      worker.join();
    }
  }

  // clear the workers
  workers.clear();
}

// worker loop
void ThreadPool::workerLoop() {
  while (true) {
    std::function<void()> job;

    // get a job from the queue
    {
      std::unique_lock<std::mutex> lock(queueMutex);

      // wait for a job or the thread pool is stopping
      condition.wait(lock, [this] { return stopping || !tasks.empty(); });
      if (stopping && tasks.empty()) {
        return;
      }

      // get the job from the queue and remove it from the queue
      job = std::move(tasks.front());
      tasks.pop();
    }

    // execute the job
    job();
  }
}
