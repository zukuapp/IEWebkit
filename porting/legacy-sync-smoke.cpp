#include <atomic>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#ifdef IEWK_REAL_WIN32
#include <windows.h>
#include "probe-telemetry.h"
#define RELEASE_ASSERT(value) do { if (!(value)) ExitProcess(90); } while (0)
#else
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
using DWORD=uint32_t;
constexpr DWORD INFINITE=0xffffffff, WAIT_OBJECT_0=0, WAIT_TIMEOUT=258;
constexpr int FALSE=0;
struct Event { std::mutex lock; std::condition_variable cv; bool signaled=false; };
using HANDLE=Event*;
static std::atomic<unsigned> liveEvents {0}, ids {0};
static std::atomic<bool> pauseWait {false}, waitPaused {false}, resumeWait {false};
static DWORD GetCurrentThreadId() { thread_local DWORD id=++ids; return id; }
static void Sleep(DWORD milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }
static HANDLE CreateEventA(void*,int manual,int initial,void*) {
    assert(!manual && !initial); ++liveEvents; return new Event;
}
static bool SetEvent(HANDLE event) {
    { std::lock_guard<std::mutex> lock(event->lock); event->signaled=true; }
    event->cv.notify_one(); return true;
}
static bool CloseHandle(HANDLE event) { delete event; --liveEvents; return true; }
static DWORD WaitForSingleObject(HANDLE event,DWORD interval) {
    if (pauseWait.exchange(false)) {
        waitPaused=true;
        while (!resumeWait) std::this_thread::yield();
    }
    std::unique_lock<std::mutex> lock(event->lock);
    bool ready;
    if (interval==INFINITE) { event->cv.wait(lock,[&] {return event->signaled;}); ready=true; }
    else ready=event->cv.wait_for(lock,std::chrono::milliseconds(interval),[&] {return event->signaled;});
    if (!ready) return WAIT_TIMEOUT;
    event->signaled=false; return WAIT_OBJECT_0;
}
#define RELEASE_ASSERT(value) assert(value)
#endif
#define ASSERT(value) RELEASE_ASSERT(value)
struct Seconds {
    double value;
    static Seconds fromMilliseconds(double v) { return {v/1000}; }
    double milliseconds() const { return value*1000; }
    bool operator>(Seconds other) const { return value>other.value; }
};
struct WallTime {
    double value;
    static WallTime now() {
#ifdef IEWK_REAL_WIN32
        return {GetTickCount()/1000.0};
#else
        return {std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count()};
#endif
    }
    static WallTime infinity() { return {HUGE_VAL}; }
    bool isInfinity() const { return std::isinf(value); }
    WallTime operator-() const { return {-value}; }
    bool operator==(WallTime other) const { return value==other.value; }
    bool operator<(WallTime other) const { return value<other.value; }
    Seconds operator-(WallTime other) const { return {value-other.value}; }
};
static WallTime after(unsigned milliseconds) { return {WallTime::now().value+milliseconds/1000.0}; }
#include "legacy-sync-types.inc"
class Mutex {
public:
    constexpr Mutex()=default;
    ~Mutex(); void lock(); bool tryLock(); void unlock();
    PlatformMutex& impl() { return m_mutex; }
    PlatformMutex m_mutex {};
};
class ThreadCondition {
public:
    constexpr ThreadCondition()=default;
    ~ThreadCondition(); void wait(Mutex&); bool timedWait(Mutex&,WallTime); void signal(); void broadcast();
    PlatformCondition m_condition {};
};
#include "legacy-sync-methods.inc"

#ifdef IEWK_REAL_WIN32
static Mutex nativeMutex;
static ThreadCondition nativeCondition;
static std::atomic<unsigned> ready {0};
static unsigned count;
static bool go;
static DWORD WINAPI worker(void*) {
    nativeMutex.lock(); ++ready;
    while (!go) nativeCondition.wait(nativeMutex);
    nativeMutex.unlock();
    for (unsigned i=0;i<5000;++i) { nativeMutex.lock(); ++count; nativeMutex.unlock(); }
    return 0;
}
int main() {
    probe_open("C:\\SYNCDIAG.LOG");
    if (!nativeMutex.tryLock() || nativeMutex.tryLock()) return probe_finish(10);
    nativeMutex.unlock();
    HANDLE threads[4];
    for (auto& thread:threads) { DWORD id=0; thread=CreateThread(nullptr,0,worker,nullptr,0,&id); if (!thread) ExitProcess(11); }
    DWORD start=GetTickCount();
    while (ready!=4) { if (GetTickCount()-start>10000) ExitProcess(12); Sleep(1); }
    nativeMutex.lock(); go=true; nativeCondition.broadcast(); nativeMutex.unlock();
    for (auto thread:threads) { if (WaitForSingleObject(thread,10000)!=WAIT_OBJECT_0) ExitProcess(13); CloseHandle(thread); }
    nativeMutex.lock();
    bool timeout=!nativeCondition.timedWait(nativeMutex,after(30));
    bool held=!nativeMutex.tryLock(); nativeMutex.unlock();
    probe_log("counter",count); probe_log("timeout",timeout); probe_log("reacquired",held);
    return probe_finish(count==20000 && timeout && held ? 0 : 14);
}
#else
template<typename Predicate> static void until(Predicate predicate) {
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while (!predicate()) { assert(std::chrono::steady_clock::now()<deadline); std::this_thread::yield(); }
}
int main() {
    Mutex mutex; ThreadCondition condition;
    assert(mutex.tryLock()); assert(!mutex.tryLock());
    bool acquired=true;
    std::thread contender([&] { acquired=mutex.tryLock(); if (acquired) mutex.unlock(); }); contender.join();
    assert(!acquired); mutex.unlock();
    unsigned counter=0; std::vector<std::thread> threads;
    for (unsigned n=0;n<4;++n) threads.emplace_back([&] { for (unsigned i=0;i<5000;++i) { mutex.lock(); ++counter; mutex.unlock(); } });
    for (auto& thread:threads) thread.join(); assert(counter==20000); threads.clear();
    puts("Mutex: nonrecursive try, cross-thread contention and 20000 increments passed");

    std::atomic<unsigned> registered {0}, awakened {0};
    for (unsigned n=0;n<2;++n) threads.emplace_back([&] { mutex.lock(); ++registered; condition.wait(mutex); assert(!mutex.tryLock()); ++awakened; mutex.unlock(); });
    until([&] {return registered==2;});
    mutex.lock(); condition.signal(); mutex.unlock();
    until([&] {return awakened==1;}); Sleep(20); assert(awakened==1);
    mutex.lock(); condition.broadcast(); mutex.unlock();
    for (auto& thread:threads) thread.join(); threads.clear(); assert(awakened==2);
    puts("Condition: signal wakes one; broadcast releases remaining waiter");

    mutex.lock(); condition.signal(); condition.broadcast();
    assert(!condition.timedWait(mutex,after(20))); assert(!mutex.tryLock());
    assert(!condition.timedWait(mutex,-WallTime::infinity()));
    assert(!condition.timedWait(mutex,{WallTime::now().value-1})); mutex.unlock();
    assert(liveEvents==0);
    puts("Timeout: mutex reacquired; notifications not stored for later waiters; expired deadlines immediate");

    pauseWait=true; waitPaused=false; resumeWait=false; bool gotSignal=false;
    std::thread beforeWait([&] {mutex.lock(); gotSignal=condition.timedWait(mutex,after(500)); mutex.unlock();});
    until([&] {return waitPaused.load();});
    mutex.lock(); condition.signal(); mutex.unlock(); resumeWait=true; beforeWait.join();
    assert(gotSignal && liveEvents==0);
    puts("No lost wake between queue registration and entering OS wait");

    for (unsigned i=0;i<100;++i) {
        std::atomic<bool> started {false};
        std::thread waiter([&] {mutex.lock(); started=true; condition.timedWait(mutex,after(2)); assert(!mutex.tryLock()); mutex.unlock();});
        until([&] {return started.load();}); Sleep(i%3);
        mutex.lock(); condition.signal(); mutex.unlock(); waiter.join();
        mutex.lock(); assert(!condition.timedWait(mutex,after(1))); mutex.unlock();
        assert(!liveEvents && !condition.m_condition.activeWaiters && !condition.m_condition.first);
    }
    puts("100 timeout/signal races: no stolen next wake or leaked event");
}
#endif
