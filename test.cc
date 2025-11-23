#include "synchronization/utils.h"
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <thread>
using namespace std;

void test_thread_self() {
  pthread_t tid1 = pthread_self();
  cout << "thread id: " << tid1 << endl;
  thread::id tid2 = this_thread::get_id();
  cout << "thread id: " << tid2 << endl;
}

void test_array_init() {}

void test_assert1() { ASSERT(1 == 2); }
std::string test_strings() {
  const char *name = "test_strings";
  std::string b{name};
  b += "```";
  cout << b << endl;
  return b;
}

int main() {
  // test_assert2();
  // test_assert1();
  test_strings();
  return 0;
}