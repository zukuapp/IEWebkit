/* Exercise the actual patched WebKit header's pair representation and atomics. */
#include <pas_utils.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

_Static_assert(sizeof(pas_pair) == 2 * sizeof(uintptr_t), "A pair holds exactly two pointers");
#ifdef IEWK_EXPECT_X86
_Static_assert(sizeof(uintptr_t) == 4, "This artifact must target x86");
#endif
static pas_pair shared_pair __attribute__((aligned(2 * sizeof(uintptr_t))));

static void work(void) {
    for (unsigned i = 0; i < 25000; i++) {
        pas_pair old, next;
        do {
            old = pas_atomic_load_pair_relaxed(&shared_pair);
            uintptr_t value = pas_pair_low(old) + 1;
            next = pas_pair_create(value, ~value);
        } while (!pas_compare_and_swap_pair_weak(&shared_pair, old, next));
    }
}
#ifdef _WIN32
static DWORD WINAPI worker(LPVOID unused) { (void)unused; work(); return 0; }
#else
static void *worker(void *unused) { (void)unused; work(); return NULL; }
#endif

int main(void) {
    uintptr_t high = (uintptr_t)~(uintptr_t)0;
    pas_pair initial = pas_pair_create(0, high);
    if (pas_pair_low(initial) != 0 || pas_pair_high(initial) != high) return 1;
    if ((uintptr_t)&shared_pair % (2 * sizeof(uintptr_t))) return 2;
    pas_atomic_store_pair(&shared_pair, initial);
    pas_pair rejected = pas_compare_and_swap_pair_strong(&shared_pair, pas_pair_create(1, 1), 0);
    if (pas_pair_low(rejected) != 0 || pas_pair_high(rejected) != high) return 3;
#ifdef _WIN32
    HANDLE threads[2] = {CreateThread(NULL, 0, worker, NULL, 0, NULL), CreateThread(NULL, 0, worker, NULL, 0, NULL)};
    if (!threads[0] || !threads[1]) return 4;
#else
    pthread_t threads[2];
    if (pthread_create(&threads[0], NULL, worker, NULL) || pthread_create(&threads[1], NULL, worker, NULL)) return 4;
#endif
    for (unsigned i = 0; i < 100000; i++) {
        pas_pair value = pas_atomic_load_pair_relaxed(&shared_pair);
        if (pas_pair_high(value) != (uintptr_t)~pas_pair_low(value)) return 5;
    }
#ifdef _WIN32
    if (WaitForMultipleObjects(2, threads, TRUE, 30000) != WAIT_OBJECT_0) return 6;
    CloseHandle(threads[0]); CloseHandle(threads[1]);
#else
    pthread_join(threads[0], NULL); pthread_join(threads[1], NULL);
#endif
    pas_pair final = pas_atomic_load_pair_relaxed(&shared_pair);
    if (pas_pair_low(final) != 50000 || pas_pair_high(final) != (uintptr_t)~(uintptr_t)50000) return 7;
    puts("PAS pair: layout, roundtrip, failed CAS, concurrent CAS/load/store passed");
    return 0;
}
