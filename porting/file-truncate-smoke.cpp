#include <cassert>
#include <cstdint>
#include <cstdio>
#include <optional>
#ifdef IEWK_REAL_WIN32
#include <windows.h>
#include "probe-telemetry.h"
#else
using DWORD = uint32_t;
using HANDLE = int;
constexpr DWORD NO_ERROR = 0;
constexpr DWORD ERROR_INVALID_PARAMETER = 87;
constexpr DWORD ERROR_ACCESS_DENIED = 5;
constexpr DWORD ERROR_SEEK = 25;
static DWORD error;
static void SetLastError(DWORD value) { error = value; }
static DWORD GetLastError() { return error; }
struct FILE_END_OF_FILE_INFO { struct { int64_t QuadPart; } EndOfFile; };
constexpr int FileEndOfFileInfo = 6;
static int seeks, failSeek, eofCalls, modernCalls;
static bool failEOF;
static uint64_t position, fileSize;
static bool SetEndOfFile(HANDLE) {
    ++eofCalls;
    if (failEOF) { SetLastError(ERROR_ACCESS_DENIED); return false; }
    fileSize = position;
    return true;
}
static bool SetFileInformationByHandle(HANDLE, int, FILE_END_OF_FILE_INFO* value, unsigned) {
    ++modernCalls;
    if (failEOF) { SetLastError(ERROR_ACCESS_DENIED); return false; }
    fileSize = value->EndOfFile.QuadPart;
    return true;
}
#endif

enum class FileSeekOrigin { Beginning, Current, End };
class FileHandle {
public:
    std::optional<HANDLE> m_handle;
    bool truncate(int64_t);
    std::optional<uint64_t> seek(int64_t offset, FileSeekOrigin origin) {
#ifdef IEWK_REAL_WIN32
        LARGE_INTEGER value;
        value.QuadPart = offset;
        DWORD mode = origin == FileSeekOrigin::Current ? FILE_CURRENT : FILE_BEGIN;
        value.LowPart = SetFilePointer(*m_handle, value.LowPart, &value.HighPart, mode);
        if (value.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
            return {};
        return value.QuadPart;
#else
        if (++seeks == failSeek) { SetLastError(ERROR_SEEK); return {}; }
        if (origin == FileSeekOrigin::Beginning) position = offset;
        else position += offset;
        return position;
#endif
    }
};
#include "file-truncate-under-test.inc"

#ifdef IEWK_REAL_WIN32
static int runProbe() {
    HANDLE native = CreateFileA("C:\\IEWKTRNC.TMP", GENERIC_READ | GENERIC_WRITE, 0,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (native == INVALID_HANDLE_VALUE) return 10;
    FileHandle handle { native };
    const char data[] = "0123456789";
    DWORD written = 0;
    int result = 0;
    if (!WriteFile(native, data, 10, &written, nullptr) || written != 10) result = 11;
    if (!result && (handle.seek(7, FileSeekOrigin::Beginning) != 7 || !handle.truncate(3)
        || handle.seek(0, FileSeekOrigin::Current) != 7 || GetFileSize(native, nullptr) != 3)) result = 12;
    if (!result && (!handle.truncate(20) || handle.seek(0, FileSeekOrigin::Current) != 7
        || GetFileSize(native, nullptr) != 20)) result = 13;
    if (!result && (!handle.truncate(0) || GetFileSize(native, nullptr))) result = 14;
    if (!result && (handle.truncate(-1) || GetLastError() != ERROR_INVALID_PARAMETER)) result = 15;
    CloseHandle(native);
    if (!DeleteFileA("C:\\IEWKTRNC.TMP") && !result) result = 16;
    return result;
}
int main() {
    probe_open("C:\\TRUNCDIAG.LOG");
    return probe_finish(runProbe());
}
#else
static void reset() {
    position = 7; fileSize = 10; seeks = failSeek = eofCalls = modernCalls = 0;
    failEOF = false; error = 0;
}
int main() {
    FileHandle handle { 1 };
#if _WIN32_WINNT < 0x0600
    for (int64_t size : {int64_t(3), int64_t(20), int64_t(0), int64_t(0x100000005)}) {
        reset(); assert(handle.truncate(size)); assert(fileSize == uint64_t(size));
        assert(position == 7 && seeks == 3 && eofCalls == 1 && modernCalls == 0);
    }
    reset(); assert(!handle.truncate(-1)); assert(!seeks && !eofCalls && error == ERROR_INVALID_PARAMETER);
    reset(); handle.m_handle.reset(); assert(!handle.truncate(3)); assert(!seeks); handle.m_handle = 1;
    for (int failure : {1, 2}) {
        reset(); failSeek = failure; assert(!handle.truncate(3));
        assert(position == 7 && fileSize == 10 && !eofCalls && error == ERROR_SEEK);
    }
    reset(); failEOF = true; assert(!handle.truncate(3));
    assert(position == 7 && fileSize == 10 && seeks == 3 && error == ERROR_ACCESS_DENIED);
    reset(); failSeek = 3; assert(!handle.truncate(3));
    assert(fileSize == 3 && position == 3 && error == ERROR_SEEK);
    reset(); failSeek = 3; failEOF = true; assert(!handle.truncate(3));
    assert(fileSize == 10 && error == ERROR_ACCESS_DENIED);
    puts("Pre-Vista truncate: 11 offset/cursor/error cases passed");
#else
    reset(); assert(handle.truncate(3)); assert(position == 7 && fileSize == 3 && modernCalls == 1 && !seeks && !eofCalls);
    reset(); failEOF = true; assert(!handle.truncate(3)); assert(error == ERROR_ACCESS_DENIED && !seeks);
    puts("Modern truncate: original API success/failure cases passed");
#endif
}
#endif
