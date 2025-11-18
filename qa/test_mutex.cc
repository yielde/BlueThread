#include "synchronization/mutex.h"
#include "synchronization/mutex_recursive.h"
#include <iostream>

using namespace std;

static void test_lock(){
    BlueMutex mutex("test_mutex", true);
    mutex.lock();
    cout << "locked" << endl;
    mutex.unlock();
    cout << "unlocked" << endl;
}

static void test_lock_recursive() {
  mutex_recursive mutex_recursive;
  mutex_recursive.lock();
  cout << "locked 1" << endl;
  mutex_recursive.lock();
  cout << "locked 2" << endl;
  mutex_recursive.unlock();
  cout << "unlocked 2" << endl;
  mutex_recursive.unlock();
  cout << "unlocked 1" << endl;
}

int main(int argc, char **argv) {
  //   test_lock();
  test_lock_recursive();
  return 0;
}