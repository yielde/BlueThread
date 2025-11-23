#include "utils.h"
#include <csignal>
#include <pthread.h>
#include <signal.h>

void block_all_signals(sigset_t *old_sigmask) {
    sigset_t sigset;
    sigfillset(&sigset);
    int r = pthread_sigmask(SIG_BLOCK, &sigset, old_sigmask);
    ASSERT(r == 0);
}

void restore_signals(sigset_t *old_sigmask) {
    int r = pthread_sigmask(SIG_SETMASK, old_sigmask, NULL);
    ASSERT(r == 0);
}

void stderr_emergency(const char *msg) {
    std::cerr << msg << std::endl;
    std::cerr.flush();
}