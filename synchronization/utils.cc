#include "utils.h"

#include <pthread.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#define MAX_LOCKS 4096

static char lock_ids_bitmap[MAX_LOCKS / 8]; // bit set to 1 means lock_id is
    // free, 0 means lock_id is used
static bool lock_ids_bitmap_initialized = false;
static int last_free_lock_id = -1;
static pthread_mutex_t lockdep_mutex = PTHREAD_MUTEX_INITIALIZER;
static std::unordered_map<std::string, int> name_to_id_map;
static std::map<int, std::string> id_to_name_map;
static std::map<int, int> lock_ref; // reference count of the lock
static std::unordered_map<pthread_t, std::map<int, BackTrace*>> held_locks;
static char lock_tree[MAX_LOCKS]
                     [MAX_LOCKS / 8]; // lock_tree[a][b] means b taken after a
static BackTrace* lock_tree_backtrace[MAX_LOCKS][MAX_LOCKS];
static unsigned current_max_id;

// Initialize lock_ids_bitmap to all 1s (all IDs free) on first use
static void
init_lock_ids_bitmap()
{
  if (!lock_ids_bitmap_initialized) {
    memset(lock_ids_bitmap, 0xFF, sizeof(lock_ids_bitmap));
    lock_ids_bitmap_initialized = true;
  }
}

// be called with a mutex held
int
lockdep_get_free_id()
{
  init_lock_ids_bitmap();
  if ((last_free_lock_id >= 0) && (lock_ids_bitmap[last_free_lock_id / 8] &
                                   (1 << (last_free_lock_id % 8)))) {
    int tmp = last_free_lock_id;
    last_free_lock_id = -1;
    // set the bit to 0, meaning the lock_id is used
    lock_ids_bitmap[tmp / 8] &= 255 - (1 << (tmp % 8));
    return tmp;
  }

  for (int i = 0; i < MAX_LOCKS / 8; i++) {
    for (int j = 0; j < 8; j++) {
      if (lock_ids_bitmap[i] & (1 << j)) {
        // set the bit to 0, meaning the lock_id is used
        lock_ids_bitmap[i] &= 255 - (1 << j);
        return i * 8 + j;
      }
    }
  }

  return -1; // can't find a free lock_id
}

static int
_lockdep_register(const std::string& name)
{
  int id = -1;
  std::unordered_map<std::string, int>::iterator p =
      name_to_id_map.find(name.c_str());
  if (p == name_to_id_map.end()) {
    id = lockdep_get_free_id();
    if (id < 0) {
      std::cerr << "Failed to get a free lock_id" << std::endl;
      return -1; // TODO:need to receive return value
    }
    if (current_max_id <= (unsigned)id) {
      current_max_id = (unsigned)id + 1;
    }
    name_to_id_map[name] = id;
    id_to_name_map[id] = name;
    std::cout << "Register lock: " << name << " with id: " << id << std::endl;
  } else {
    id = p->second;
    std::cout << "Lock already registered: " << name << " with id: " << id
              << std::endl;
  }
  lock_ref[id]++;
  return id;
}

/*
critical detection:

does_follow(5, 1)
  ├─ 直接依赖？follows[5][1]？没有
  └─ 检查间接依赖：
     遍历 i = 0 到 current_maxid
       ├─ i=1: follows[5][1]？没有，跳过
       ├─ i=2: follows[5][2]？没有，跳过
       ├─ i=3: follows[5][3]？没有，跳过
       ├─ i=4: follows[5][4]？有！递归检查
       │         └─ does_follow(4, 1)
       │             ├─ 直接依赖？follows[4][1]？没有
       │             └─ 检查间接依赖：
       │                 ├─ i=3: follows[4][3]？有！递归检查
       │                 │         └─ does_follow(3, 1)
       │                 │             ├─ 直接依赖？follows[3][1]？没有
       │                 │             └─ 检查间接依赖：
       │                 │                 ├─ i=2: follows[3][2]？有！递归检查
       │                 │                 │         └─ does_follow(2, 1)
       │                 │                 │             ├─
直接依赖？follows[2][1] = 1 ✅ 找到！ │                 │                 │ └─
返回 true │                 │                 └─ 返回 true（路径：2 -> 1） │ └─
返回 true（路径：3 -> 2 -> 1） └─ 返回 true（路径：4 -> 3 -> 2 -> 1）

最终返回 true（路径：5 -> 4 -> 3 -> 2 -> 1）
*/
static bool
does_follow(int a, int b)
{
  std::ostream& os = std::cout;
  if (lock_tree[a][b / 8] & (1 << (b % 8))) {
    os << "----------------------------"
       << "\n";
    os << "dependency exists "
       << "【" << a << ": " << id_to_name_map[a] << "】"
       << " -> "
       << "【" << b << ": " << id_to_name_map[b] << "】"
       << "\n";
    if (lock_tree_backtrace[a][b]) {
      lock_tree_backtrace[a][b]->print(os);
    }
    os << std::endl;
    return true;
  }

  for (unsigned i = 0; i < current_max_id; i++) {
    if ((lock_tree[a][i / 8] & (1 << (i % 8))) && does_follow(i, b)) {
      os << "intermediate dependency exists "
         << "【" << a << ": " << id_to_name_map[a] << "】"
         << " -> "
         << "【" << i << ": " << id_to_name_map[i] << "】"
         << " -> "
         << "【" << b << ": " << id_to_name_map[b] << "】"
         << "\n";
      if (lock_tree_backtrace[a][i]) {
        lock_tree_backtrace[a][i]->print(os);
      }
      os << std::endl;
      // direct dependency exists
      return true;
    }
  }
  return false;
}

int
lockdep_will_lock(const std::string& name, int id)
{

  pthread_t tid = pthread_self();
  pthread_mutex_lock(&lockdep_mutex);
  if (id < 0) {
    id = _lockdep_register(name);
    if (id < 0) {
      pthread_mutex_unlock(&lockdep_mutex);
      throw std::runtime_error("Failed to register lock: " + name);
    }
  }

  std::cout << "Lock: " << name << " with id: " << id
            << " will lock by thread: " << tid << std::endl;
  auto& lockid_backtrace_map = held_locks[tid];
  for (auto p = lockid_backtrace_map.begin(); p != lockid_backtrace_map.end();
       p++) {
    if (p->first == id) {
      // lock is already held by the thread
      std::ostream& os = std::cout;
      os << "\n";
      os << "Lock: " << name << " with id: " << id
         << " is already held by thread: " << tid << std::endl;
      auto bt = new BackTrace(0);
      bt->print(os);
      if (p->second) {
        os << "previously locked at: ";
        p->second->print(os);
      }
      delete bt;
      os << std::endl;
      abort();
    } else if (!(lock_tree[p->first][id / 8] & (1 << (id % 8)))) {
      // add new lockdep
      std::ostream& os = std::cout;
      // Check for potential deadlock: if there's a path from 'id' to
      // 'p->first', then adding 'p->first' -> 'id' would create a cycle
      if (does_follow(id, p->first)) {
        // will cause deadlock - there's already a path from id to p->first,
        // so adding p->first -> id creates a cycle

        os << "new dependency "
           << "【" << p->first << ": " << id_to_name_map[p->first] << "】"
           << " -> "
           << "【" << id << ": " << id_to_name_map[id] << "】"
           << " will cause deadlock (cycle detected)";
        os << std::endl;
        auto bt = new BackTrace(0);
        bt->print(os);
        std::cout << "thread " << tid << " hold locks: " << std::endl;
        for (auto q = lockid_backtrace_map.begin();
             q != lockid_backtrace_map.end(); q++) {
          std::cout << "【" << q->first << ": " << id_to_name_map[q->first]
                    << "】" << std::endl;
          if (q->second) {
            q->second->print(os);
            os << std::endl;
          }
        }
        // std::cout << std::endl;
        abort();
      } else {
        BackTrace* bt = new BackTrace(0);
        lock_tree[p->first][id / 8] |= (1 << (id % 8));
        lock_tree_backtrace[p->first][id] = bt;

        std::cout << "new dependency "
                  << "【" << p->first << ": " << id_to_name_map[p->first]
                  << "】"
                  << " -> "
                  << "【" << id << ": " << id_to_name_map[id] << "】"
                  << std::endl;
      }
    }
  }
  pthread_mutex_unlock(&lockdep_mutex);
  return id;
}

int
lockdep_register(const std::string& name)
{
  int id = -1;
  pthread_mutex_lock(&lockdep_mutex);
  id = _lockdep_register(name);
  if (id < 0) {
    pthread_mutex_unlock(&lockdep_mutex);
    throw std::runtime_error("Failed to register lock: " + name);
  }
  pthread_mutex_unlock(&lockdep_mutex);
  return id;
}

int
lockdep_locked(const std::string& name, int id)
{
  pthread_t tid = pthread_self();
  pthread_mutex_lock(&lockdep_mutex);
  if (id < 0) {
    id = _lockdep_register(name);
    if (id < 0) {
      pthread_mutex_unlock(&lockdep_mutex);
      throw std::runtime_error("Failed to register lock: " + name);
    }
  }
  std::cout << "Lock: " << name << " with id: " << id
            << "_locked by thread: " << tid << std::endl;
  held_locks[tid][id] = new BackTrace(0);
  pthread_mutex_unlock(&lockdep_mutex);
  return id;
}

void
lockdep_unregister(int id)
{
  if (id < 0) {
    return;
  }
  pthread_mutex_lock(&lockdep_mutex);
  std::string name;
  auto p = id_to_name_map.find(id);
  if (p == id_to_name_map.end()) {
    name = "unknown";
  } else {
    name = p->second;
  }

  int& ref = lock_ref[id];
  if (--ref == 0) {
    if (p != id_to_name_map.end()) {
      memset((void*)&lock_tree[id][0], 0, MAX_LOCKS / 8);
      for (unsigned i = 0; i < current_max_id; i++) {
        delete lock_tree_backtrace[id][i];
        lock_tree_backtrace[id][i] = nullptr;

        delete lock_tree_backtrace[i][id];
        lock_tree_backtrace[i][id] = nullptr;

        lock_tree[i][id / 8] &= 255 - (1 << (id % 8));
      }
      std::cout << "unregister lock: " << name << " with id: " << id
                << std::endl;
      id_to_name_map.erase(id);
      name_to_id_map.erase(name);
    }
    lock_ref.erase(id);
    lock_ids_bitmap[id / 8] |= (1 << (id % 8));
    last_free_lock_id = id;
  }

  pthread_mutex_unlock(&lockdep_mutex);
}

int
lockdep_will_unlock(const std::string& name, int id)
{
  pthread_t tid = pthread_self();
  if (id < 0) {
    ASSERT(id == -1);
    return id;
  }
  pthread_mutex_lock(&lockdep_mutex);
  std::cout << "Will unlock lock: " << name << " with id: " << id
            << " by thread: " << tid << std::endl;
  delete held_locks[tid][id];
  held_locks[tid].erase(id);
  pthread_mutex_unlock(&lockdep_mutex);
  return id;
}