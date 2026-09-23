/* Build-time import fixture for APIs referenced by the pinned modern WTF port.
 * This is not an engine or an executable intended for end users. */
#include <windows.h>

static void probe(void) {
    SRWLOCK lock = SRWLOCK_INIT;
    CONDITION_VARIABLE condition = CONDITION_VARIABLE_INIT;
    ULONG_PTR low, high;
    FILE_END_OF_FILE_INFO end;
    /* CurrentTime.cpp selects the existing ME API on i386. */
    volatile DWORD clock = GetTickCount();
    (void)clock;
    AcquireSRWLockExclusive(&lock);
    SleepConditionVariableSRW(&condition, &lock, 1, 0);
    WakeConditionVariable(&condition);
    WakeAllConditionVariable(&condition);
    ReleaseSRWLockExclusive(&lock);
    TryAcquireSRWLockExclusive(&lock);
    GetCurrentThreadStackLimits(&low, &high);
    end.EndOfFile.QuadPart = 0;
    SetFileInformationByHandle(INVALID_HANDLE_VALUE, FileEndOfFileInfo, &end, sizeof end);
}
/* Retain the code for import inspection without executing it. */
void (*volatile iewebkit_required_api_fixture)(void) = probe;
int main(void) { return 0; }
