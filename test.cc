#include <mutex>
#include <pthread.h>
#include <thread>
#include <iostream>

int main(){
    std::thread::id id = std::this_thread::get_id();
    std::cout << "Thread id: " << id << std::endl;
    pthread_t id2 = pthread_self();
    std::cout << "Thread id2: " << id2 << std::endl;
    return 0;
}