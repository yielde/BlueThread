#ifndef UTILS_H
#define UTILS_H
#include <bits/types/sigset_t.h>
#include <stdlib.h>

#include <iostream>

#define ASSERT(x)                                                          \
  do {                                                                     \
    if (!(x)) {                                                            \
      std::cerr << "Assertion failed: " << #x << " at " << __FILE__ << ":" \
                << __LINE__ << ":" << __func__ << std::endl;               \
      abort();                                                             \
    }                                                                      \
  } while (0)
#endif

void block_all_signals(sigset_t* old_sigmask);
void restore_signals(sigset_t* old_sigmask);

void stderr_emergency(const char* msg);