#include "shared_mutex.h"
#include "utils.h"
#include <pthread.h>

shared_mutex_debug::shared_mutex_debug(std::string name, bool lockdep,
                                       bool prioritize_write)
    : mutex_debug_base(std::move(name), lockdep) {
  if (prioritize_write) {
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    pthread_rwlockattr_setkind_np(&attr,
                                  PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    pthread_rwlock_init(&rwlock, &attr);
    pthread_rwlockattr_destroy(&attr);
  } else {
    pthread_rwlock_init(&rwlock, NULL);
  }
}

shared_mutex_debug::~shared_mutex_debug() { pthread_rwlock_destroy(&rwlock); }

void shared_mutex_debug::lock() {
  if (lockdep) {
    _will_lock();
  }
  if (int r = pthread_rwlock_wrlock(&rwlock); r != 0) {
    throw std::system_error(r, std::generic_category(),
                            "pthread_rwlock_wrlock");
  }
  if (lockdep) {
    _locked();
  }
  _post_lock();
}

void shared_mutex_debug::unlock() {
  _pre_unlock();
  if (lockdep) {
    _will_unlock();
  }
  if (int r = pthread_rwlock_unlock(&rwlock); r != 0) {
    throw std::system_error(r, std::generic_category(), "pthread_rwlock_unlock");
  }
}

void shared_mutex_debug::lock_shared() {
  if (lockdep) {
    _will_lock();
  }
  if (int r = pthread_rwlock_rdlock(&rwlock); r != 0) {
    throw std::system_error(r, std::generic_category(), "pthread_rwlock_rdlock");
  }
  if (lockdep) {
    _locked();
  }
  _post_lock_shared();
}

void shared_mutex_debug::unlock_shared() {
  _pre_unlock_shared();
  if (lockdep) {
    _will_unlock();
  }
  if (int r = pthread_rwlock_unlock(&rwlock); r != 0) {
    throw std::system_error(r, std::generic_category(), "pthread_rwlock_unlock");
  }
}

void shared_mutex_debug::_pre_unlock() {
  if (lockdep) {
    ASSERT(locked_count > 0);
    --locked_count;
    ASSERT(locked_by == std::this_thread::get_id());
    ASSERT(locked_count == 0);
  }
}

void shared_mutex_debug::_post_lock() {
  if (lockdep) {
    ASSERT(locked_count == 0);
    ++locked_count;
    locked_by = std::this_thread::get_id();
  }
}

void shared_mutex_debug::_pre_unlock_shared() {
  if (lockdep) {
    ASSERT(read_locked_count > 0);
    --read_locked_count;
  }
}

void shared_mutex_debug::_post_lock_shared() {
  if (lockdep) {
    ++read_locked_count;
  }
}