#include "utils.h"

#define MAX_LOCKS 4096

static char lock_ids[MAX_LOCKS / 8]; // bit set to 1 means lock_id is free, 0
                                     // means lock_id is used
static int last_free_lock_id = -1;

// be called with a mutex held
int lockdep_get_free_id() {
  if ((last_free_lock_id >= 0) &&
      (lock_ids[last_free_lock_id / 8] & (1 << (last_free_lock_id % 8)))) {
    int tmp = last_free_lock_id;
    last_free_lock_id = -1;
    lock_ids[tmp / 8] &=
        255 - (1 << (tmp % 8)); // set the bit to 0, meaning the lock_id is used
    return tmp;
  }

  for (int i = 0; i < MAX_LOCKS / 8; i++) {
    for (int j = 0; j < 8; j++) {
      if (lock_ids[i] & (1 << j)) {
        lock_ids[i] &=
            255 - (1 << j); // set the bit to 0, meaning the lock_id is used
        return i * 8 + j;
      }
    }
  }
  
  return -1; // can't find a free lock_id
}
