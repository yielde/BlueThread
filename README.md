# BlueThread
Unraveling Ceph’s Multithreading Framework, based Pacific

# synchronization primitive
## mutex
Implement using pthread_mutex_t(PTHREAD_MUTEX_ERRORCHECK).
* lockdep can detect self-deadlock and circular deadlock

## recursive mutex
Implement using pthread_mutex_t(PTHREAD_MUTEX_RECURSIVE).

## condition_variable
Rely on mutex



# concurrency
## thread
Implement using pthread_t, and also implement a simple functiontool supporting std::thread.

## threadpool
