#include <pthread.h>
#include <string>


class BlueThread {
    private:
    pthread_t tid;
    std::string thread_name;

};