#ifdef IEWK_PROCESS_HEADER_FIRST
#include "pas_process.h"
#endif
#include <memory>
#include <mutex>
#include <pthread.h>
#include <type_traits>
#ifndef IEWK_PROCESS_HEADER_FIRST
#include "pas_process.h"
#endif
#ifdef pthread_t
#error The process query API must not replace the CRT pthread_t type with a macro
#endif
static_assert(std::is_same_v<decltype(&pas_process_is_shutting_down), bool (*)(void)>);
static_assert(std::is_same_v<decltype(PTHREAD_ONCE_INIT), int>);
int main() {
    // Call the actual C implementation, verifying unmangled linkage.
    return pas_process_is_shutting_down() ? 1 : 0;
}
