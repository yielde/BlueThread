#ifndef BACKTRACE_CC
#define BACKTRACE_CC
#include "backtrace.h"
#include <cstddef>
#include <cxxabi.h>

void BackTrace::print(std::ostream &os) const {
  for (size_t i = skip; i < size; i++) {
    os << (i - skip + 1) << ": " << demangle(strings[i]) << std::endl;
  }
}

std::string BackTrace::demangle(const char *name) {
  static constexpr char OPEN = '(';
  const char *begin = nullptr;
  const char *end = nullptr;
  // get function name pointer between '(' and '+'
  for (const char *p = name; *p; ++p) {
    if (*p == OPEN) {
      begin = p + 1;
    } else if (*p == '+') {
      end = p;
    }
  }

  if (begin && end && begin < end) {
    std::string mangled(begin, end);
    int status;
    if (mangled.compare(0, 2, "_Z") == 0) {
      char *demangled =
          abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
      if (status == 0) {
        std::string full_name{OPEN};
        full_name += demangled;
        full_name += end;
        free(demangled);
        return full_name;
      }
    }
    // C function don't need to demangle
    return mangled + "()";
  } else {
    return name;
  }
}
#endif