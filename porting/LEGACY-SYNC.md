# Pre-Vista mutex and condition checkpoint

The actual ME `ThreadingWin.cpp` object still failed at SRW and condition-variable
APIs after the earlier include/SEH fixes. The hash-pinned pre-Vista branch now
compiles that object and the generic `Threading.cpp`. Vista+ keeps its SRW and
native condition types and methods. Existing debugger-naming changes are retained
in the cumulative `webkit-2.54.0-threading-win.patch`; each manifest path has one
pristine-to-current patch/hash. The separate type patch changes only the older
Windows ABI.

## Lock and wait contract

- A constexpr, lock-free DWORD atomic stores the mutex owner. A successful CAS
  acquires, unlock publishes a release store, and failed try-lock returns
  immediately, including recursive attempts. Blocking contention sleeps 1ms.
  This is not a recursive mutex. There is no FIFO guarantee, priority inheritance,
  abandoned-owner recovery or fairness claim. Destroying a held mutex asserts.
- Each condition wait creates a private auto-reset event and puts a stack-owned
  waiter in a FIFO queue protected by an internal mutex. Queue registration
  precedes releasing the caller's predicate mutex, closing the lost-wakeup gap.
- Signal removes one queued waiter; broadcast removes the currently queued set.
  An event belongs to one waiter only. Notifications are not saved for future
  waits. SetEvent completes under the queue lock before timeout cleanup may
  close/recycle the event handle.
- Timeout cleanup removes an unsignaled waiter while holding the same lock.
  If a signal has already selected that waiter, it wins the race and the wait
  reports signaled. Otherwise the timeout wins and later signals select another
  waiter. The caller must recheck its predicate, as with ordinary condition waits.
- Every completed OS wait closes its event and reacquires the caller mutex.
  Existing zero/past-deadline behavior returns false while retaining that mutex.
  Resource/API failure fails closed through a release assertion. Cancellation
  and forced thread termination are not implemented. Destruction requires no
  active waiters; owners must finish/join users before destroying shared state.

Ordinary mutex operations allocate no handle or heap object. Condition waits
allocate one event each, so kernel resource exhaustion is a real runtime limit.
The backend does not use the ME `TryEnterCriticalSection` stub previously found
by the allocator fixture.

The event state and handle-lifetime rules follow Microsoft’s
[SetEvent](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setevent)
and [WaitForSingleObject](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject)
contracts. These modern documents do not replace a WinME runtime check.

## Reproduce

```
python3 porting/apply-webkit-patches.py --source /external/webkitgtk-2.54.0
python3 porting/build-legacy-sync-probes.py \
    --source /external/webkitgtk-2.54.0 --output /external/sync-probes
ninja -C /external/build-me-jsc -j1 \
    Source/WTF/wtf/CMakeFiles/WTF.dir/win/ThreadingWin.cpp.obj \
    Source/WTF/wtf/CMakeFiles/WTF.dir/Threading.cpp.obj
```

The fixture extracts actual type declarations and methods after checking both
source hashes. Host Win32 event functions are implemented with Linux mutexes
and condition variables; the production queue and owner code is unchanged.
ME and NT5 declaration runs both pass nonrecursive/cross-thread try-lock,
20,000 protected increments, one-waiter signal, broadcast, expired timeout and
mutex reacquisition, no stored notifications, deterministic notification before
the OS wait starts, and 100 timeout/signal races with no remaining queue/event.
Clang AddressSanitizer+UBSan also passed these tests. GCC's sanitizer runtime
was absent on this host; no GCC sanitizer result is claimed.

The full actual ME Windows-thread object compiled, SHA-256
`63f4f8eb8b5bd625434e2e3124889e5c14f0e86def8139a99fa488ae0b2b2485`.
The modern-declaration full object also compiles and retains SRW/condition imports.
The naming regression (host ordering plus real MSVC x86/x64 SEH syntax) and nine
patch/bootstrap unit tests pass. This does not prove the complete WTF library
or JSC links or runs.

The static x86 native fixture passes the pinned ME DLL import audit with no gaps,
SHA-256 `e2d3a7af8e02190ff147d969459afaf0324e3c710df7ff4d766f023a61bf0eb3`.
It is built to test four real workers, broadcast, 20,000 increments and timed
wait/reacquisition, logging `C:\SYNCDIAG.LOG`.

On the actual Windows ME 4.90.3000 guest with IE5.5, the fixture ran from a
read-only ISO in a no-NIC QEMU overlay. ISO SHA-256:
`8988f0b922c935f786d646b9635fd8e11a3d44632ef68926dbf33faee3135a18`.
The fixed launcher recorded `launched=1`, `wait=0`, and `exit.code=0`. The
guest-created `SYNCDIAG.LOG` (pulled via the bounded COM1 QA channel; SHA-256
`45bb980af467d09b14dd5d713edcc3c2b0f2e9f8962f30579bd92f966c2b0ee1`)
recorded `counter=0x4e20` (20,000), `timeout=1`, `reacquired=1`,
`last_error=0`, and `exit=0`. The VM drive listing and launcher log are stored
under `/srv/zuku/deploy-work/20260924-legacy-sync/`. This is an actual ME
runtime pass for this native fixture, not an entire WTF/JSC link or engine test.
The event shim, sanitizer pass and import table do not certify ME event/scheduler
behavior, thread TLS cleanup, suspend/resume or engine garbage-collector races.
The actual object still references other thread APIs such as `SwitchToThread`
and CRT `_beginthreadex`; these need final linked-import and lifetime validation.
External logs/artifacts: `/srv/zuku/deploy-work/20260924-legacy-sync/`.
