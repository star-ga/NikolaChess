
#include "thread_affinity.h"
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif
#include <cstdlib>

bool nikola_pin_thread_to_core(int coreIndex) {
#ifdef __linux__
    if (coreIndex < 0) return false;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(coreIndex, &cpuset);
    pthread_t thread = pthread_self();
    int rc = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    return rc == 0;
#else
    (void)coreIndex; return false;
#endif
}
