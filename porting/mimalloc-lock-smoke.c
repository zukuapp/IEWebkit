/* Test the actual mimalloc lock branch selected for the target declarations. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <mimalloc/atomic.h>

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
int main(void) {
    mi_lock_init(&lock);
    if (!mi_lock_try_acquire(&lock)) return 1;
    checked = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!checked) return 2;
    HANDLE first = CreateThread(NULL, 0, try_worker, NULL, 0, NULL);
    if (!first) return 3;
    HANDLE either[2] = {checked, first};
    if (WaitForMultipleObjects(2, either, FALSE, 10000) != WAIT_OBJECT_0) return 4;
    if (counter != 0 || WaitForSingleObject(first, 0) != WAIT_TIMEOUT) return 5;
    mi_lock_release(&lock);
    if (WaitForSingleObject(first, 10000) != WAIT_OBJECT_0) return 6;
    DWORD code;
    if (!GetExitCodeThread(first, &code) || code != 0 || counter != 1) return 7;
    CloseHandle(first);
    CloseHandle(checked);
    HANDLE workers[2] = {CreateThread(NULL, 0, increment_worker, NULL, 0, NULL),
                         CreateThread(NULL, 0, increment_worker, NULL, 0, NULL)};
    if (!workers[0] || !workers[1]) return 8;
    if (WaitForMultipleObjects(2, workers, TRUE, 30000) != WAIT_OBJECT_0) return 9;
    CloseHandle(workers[0]); CloseHandle(workers[1]);
    if (counter != 100001 || !mi_lock_try_acquire(&lock)) return 10;
    mi_lock_release(&lock);
    mi_lock_done(&lock);
    puts("mimalloc lock: failed try, blocking handoff, contention and teardown passed");
    return 0;
}
