#pragma once
#include "mutex.h"
#include <bits/types/struct_timespec.h>
#include <condition_variable>
#include <mutex>
#include <pthread.h>

class condition_variable {
private:
  BlueMutex *waiter_mutex;
  pthread_cond_t cond;

public:
  condition_variable() : waiter_mutex{nullptr} {
    int r = pthread_cond_init(&cond, nullptr);
    if (r) {
      throw std::system_error(r, std::generic_category());
    }
  }
  ~condition_variable() { pthread_cond_destroy(&cond); }
  condition_variable(const condition_variable &) = delete;
  condition_variable &operator=(const condition_variable &) = delete;

private:
  std::cv_status _wait_until(
      BlueMutex *mutex,
      const std::chrono::time_point<std::chrono::steady_clock> &abs_time);

public:
  void wait(std::unique_lock<BlueMutex> &lock);
  template <class Predicate>
  void wait(std::unique_lock<BlueMutex> &lock, Predicate pred) {
    while (!pred()) {
      wait(lock);
    }
  }

  template <typename Clock, typename Duration>
  std::cv_status wait_until(std::unique_lock<BlueMutex> &lock,
                            std::chrono::time_point<Clock, Duration> &when) {
    return _wait_until(lock.mutex(), when);
  }

  template <typename Clock, typename Duration>
  std::cv_status wait_for(std::unique_lock<BlueMutex> &lock,
                          std::chrono::duration<Clock, Duration> &awhile) {
    auto when = Clock::now();
    when += awhile;
    return _wait_until(lock.mutex(), when);
  }

  template <typename Clock, typename Duration, typename Predicate>
  bool wait_for(std::unique_lock<BlueMutex> &lock,
                std::chrono::duration<Clock, Duration> &awhile,
                Predicate pred) {
    auto when = Clock::now();
    when += awhile;
    while (!pred()) {
      if (_wait_until(lock.mutex(), when) == std::cv_status::timeout) {
        return pred();
      }
    }
    return true;
  }

  void notify_one();
  void notify_all();
};

using BlueConditionVariable = condition_variable;