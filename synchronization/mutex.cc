#include "mutex.h"
#include <pthread.h>

void mutex_debug_base::_will_lock() {
    id = lockdep_will_lock(lock_name, id);
}
void mutex_debug_base::_register() { id = lockdep_register(lock_name); }
void mutex_debug_base::_unregister() { lockdep_unregister(id); }
void mutex_debug_base::_locked() { id = lockdep_locked(lock_name, id); }
void mutex_debug_base::_will_unlock() {
  id = lockdep_will_unlock(lock_name, id);
}