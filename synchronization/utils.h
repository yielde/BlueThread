#ifndef UTILS_H
#define UTILS_H
#include "backtrace.h"
#include <iostream>

#define unlikely(x) __builtin_expect((x), 0)

#define ASSERT(x)                                                              \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::cerr << "Assertion failed: " << #x << " at " << __FILE__ << ":"     \
                << __LINE__ << ":" << __func__ << std::endl;                   \
      abort();                                                                 \
    }                                                                          \
  } while (0)

int lockdep_register(const std::string &name);
int lockdep_will_lock(const std::string &name, int id);
int lockdep_locked(const std::string &name, int id);
void lockdep_unregister(int id);
int lockdep_will_unlock(const std::string &name, int id);

#endif