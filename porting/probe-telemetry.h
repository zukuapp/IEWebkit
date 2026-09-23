/* Native file telemetry remains available when Win9x console capture is empty. */
#ifndef IEWEBKIT_PROBE_TELEMETRY_H
#define IEWEBKIT_PROBE_TELEMETRY_H
#ifdef _WIN32
#include <windows.h>
static HANDLE probe_file = INVALID_HANDLE_VALUE;
static void probe_log(const char *label, DWORD value) {
    DWORD saved = GetLastError();
    char line[160];
    unsigned n = 0;
    while (*label && n < 140) line[n++] = *label++;
    line[n++] = '='; line[n++] = '0'; line[n++] = 'x';
    for (unsigned i = 0; i < 8; ++i)
        line[n++] = "0123456789abcdef"[(value >> (28 - 4 * i)) & 15];
    line[n++] = '\r'; line[n++] = '\n';
    if (probe_file != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(probe_file, line, n, &written, NULL);
        FlushFileBuffers(probe_file);
    }
    SetLastError(saved);
}
static void probe_open(const char *path) {
    probe_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    probe_log("started", 1);
}
static int probe_finish(int result) {
    probe_log("last_error", GetLastError());
    probe_log("exit", (DWORD)result);
    if (probe_file != INVALID_HANDLE_VALUE) CloseHandle(probe_file);
    return result;
}
#endif
#endif
