/* Test the actual mimalloc lock branch selected for the target declarations. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <mimalloc/atomic.h>
#include "probe-telemetry.h"

static mi_lock_t lock;
static unsigned counter;
static HANDLE checked;
static DWORD WINAPI try_worker(LPVOID unused) {
    (void)unused;
    if (mi_lock_try_acquire(&lock)) {
        mi_lock_release(&lock);
        return 1;
    }
    SetEvent(checked);
    mi_lock_acquire(&lock);
    ++counter;
    mi_lock_release(&lock);
    return 0;
}
static DWORD WINAPI increment_worker(LPVOID unused) {
    (void)unused;
    for (unsigned i = 0; i < 50000; ++i) {
        mi_lock_acquire(&lock);
        ++counter;
        mi_lock_release(&lock);
    }
    return 0;
}
static int run_probe(void) {
    mi_lock_init(&lock);
    SetLastError(0);
    bool acquired = mi_lock_try_acquire(&lock);
    probe_log("initial_try.error", GetLastError());
    probe_log("initial_try.acquired", acquired);
    if (!acquired) return 1;
#if defined(_WIN32_WINDOWS) && _WIN32_WINDOWS <= 0x0490
    /* Win9x backend retains critical-section recursion, unlike modern SRW. */
    if (!mi_lock_try_acquire(&lock)) return 11;
    mi_lock_acquire(&lock);
    probe_log("recursive_depth", 3);
#endif
    checked = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!checked) return 2;
    DWORD first_id;
    SetLastError(0);
    HANDLE first = CreateThread(NULL, 0, try_worker, NULL, 0, &first_id);
    probe_log("first_thread.error", GetLastError());
    if (!first) return 3;
    HANDLE either[2] = {checked, first};
    if (WaitForMultipleObjects(2, either, FALSE, 10000) != WAIT_OBJECT_0) return 4;
    if (counter != 0 || WaitForSingleObject(first, 0) != WAIT_TIMEOUT) return 5;
#if defined(_WIN32_WINDOWS) && _WIN32_WINDOWS <= 0x0490
    for (unsigned depth = 2; depth > 0; --depth) {
        mi_lock_release(&lock);
        if (WaitForSingleObject(first, 10) != WAIT_TIMEOUT || counter != 0) return 12;
        probe_log("recursive_depth", depth);
    }
#endif
    mi_lock_release(&lock);
    if (WaitForSingleObject(first, 10000) != WAIT_OBJECT_0) return 6;
    DWORD code;
    if (!GetExitCodeThread(first, &code) || code != 0 || counter != 1) return 7;
    CloseHandle(first);
    CloseHandle(checked);
    DWORD worker_ids[2];
    HANDLE workers[2];
    for (unsigned i = 0; i < 2; ++i) {
        SetLastError(0);
        workers[i] = CreateThread(NULL, 0, increment_worker, NULL, 0, &worker_ids[i]);
        probe_log("worker_index", i);
        probe_log("worker.error", GetLastError());
        if (!workers[i]) return 8;
    }
    if (WaitForMultipleObjects(2, workers, TRUE, 30000) != WAIT_OBJECT_0) return 9;
    CloseHandle(workers[0]); CloseHandle(workers[1]);
    if (counter != 100001 || !mi_lock_try_acquire(&lock)) return 10;
    mi_lock_release(&lock);
    mi_lock_done(&lock);
    probe_log("counter", counter);
    puts("mimalloc lock: failed try, blocking handoff, contention and teardown passed");
    return 0;
}

int main(void) {
    probe_open("C:\\LOCKDIAG.LOG");
    return probe_finish(run_probe());
}
