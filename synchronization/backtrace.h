#ifndef BACKTRACE_H
#define BACKTRACE_H
#include <execinfo.h>
#include <stdlib.h>

#include <iostream>

// store the backtrace of the current thread
class BackTrace {
public:
  const static int max_depth = 30;
  void* array[max_depth]{};
  size_t size;
  char** strings;
  int skip;

  explicit BackTrace(int s) : skip(s)
  {
    size = backtrace(array, max_depth);
    strings = backtrace_symbols(array, size);
  }

  ~BackTrace() { free(strings); }

  BackTrace(const BackTrace& other);
  const BackTrace& operator=(const BackTrace& other);

  void print(std::ostream& os) const;
  static std::string demangle(const char* name);
};

inline std::ostream&
operator<<(std::ostream& os, const BackTrace& bt)
{
  bt.print(os);
  return os;
}
#endif