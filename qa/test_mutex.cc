#include "synchronization/mutex.h"
#include <iostream>

using namespace std;

static void test_lock(){
    BlueMutex mutex("test_mutex", true);
    mutex.lock();
    cout << "locked" << endl;
    mutex.unlock();
    cout << "unlocked" << endl;
}

int main(int argc, char **argv) { test_lock(); return 0; }