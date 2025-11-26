#include "condition_variable.h"
#include "synchronization/utils.h"
#include <ctime>
#include <pthread.h>

void condition_variable::wait(std::unique_lock<BlueMutex> &lock) {
  ASSERT(waiter_mutex == nullptr || waiter_mutex == lock.mutex());
  waiter_mutex = lock.mutex();
  ASSERT(waiter_mutex->is_locked());
  waiter_mutex->_pre_unlock();
  if (int r = pthread_cond_wait(&cond, waiter_mutex->native_handle()); r != 0) {
    throw std::system_error(r, std::generic_category());
  }
  waiter_mutex->_post_lock();
}

void condition_variable::notify_one() {
  ASSERT(waiter_mutex == nullptr || waiter_mutex->is_locked());
  if (int r = pthread_cond_signal(&cond); r != 0) {
    throw std::system_error(r, std::generic_category());
  }
}

void condition_variable::notify_all() {
  ASSERT(waiter_mutex == nullptr || waiter_mutex->is_locked());
  if (int r = pthread_cond_broadcast(&cond); r != 0) {
    throw std::system_error(r, std::generic_category());
  }
}

std::cv_status condition_variable::_wait_until(BlueMutex *mutex, const std::chrono::time_point<std::chrono::steady_clock> &abs_time) {
  ASSERT(waiter_mutex == nullptr || waiter_mutex == mutex);
  waiter_mutex = mutex;
  ASSERT(waiter_mutex->is_locked());
  waiter_mutex->_pre_unlock();
  timespec ts = to_timespec<std::chrono::steady_clock>(abs_time);
  int r = pthread_cond_timedwait(&cond, waiter_mutex->native_handle(), &ts);
  waiter_mutex->_post_lock();
  switch (r) {
    case 0:
      return std::cv_status::no_timeout;
    case ETIMEDOUT:
      return std::cv_status::timeout;
    default:
      throw std::system_error(r, std::generic_category());
  }
}