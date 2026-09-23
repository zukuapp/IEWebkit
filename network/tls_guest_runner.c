/* Fixed, offline ME QA launch. No command input or general remote shell. */
/* Copyright (c) 2026 IEWebkit contributors. SPDX-License-Identifier: BSD-2-Clause */
#define WINVER 0x0410
#include <windows.h>

static void clear_bytes(void *out, DWORD count)
{
    BYTE *bytes = (BYTE *)out;
    while (count--)
        *bytes++ = 0;
}

static void record(HANDLE log, const char *key, DWORD value)
{
    char line[128];
    DWORD written;
    wsprintfA(line, "%s=%lu (0x%08lx)\r\n", key, value, value);
    WriteFile(log, line, lstrlenA(line), &written, NULL);
    FlushFileBuffers(log);
}

void WINAPI start(void)
{
    HANDLE log, output, input;
    SECURITY_ATTRIBUTES security;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    BOOL launched;
    DWORD code = 0xffffffffu, waited, launch_error;

    log = CreateFileA("C:\\ZUKUQA\\TLSRUN.LOG", GENERIC_WRITE, FILE_SHARE_READ,
                      NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log == INVALID_HANDLE_VALUE)
        ExitProcess(2);
    clear_bytes(&security, sizeof(security));
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    output = CreateFileA("C:\\ZUKUQA\\TLSOUT.LOG", GENERIC_WRITE, FILE_SHARE_READ,
                         &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    input = CreateFileA("NUL", GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                        OPEN_EXISTING, 0, NULL);
    if (output == INVALID_HANDLE_VALUE || input == INVALID_HANDLE_VALUE) {
        record(log, "io.error", GetLastError());
        if (output != INVALID_HANDLE_VALUE)
            CloseHandle(output);
        if (input != INVALID_HANDLE_VALUE)
            CloseHandle(input);
        CloseHandle(log);
        ExitProcess(2);
    }
    clear_bytes(&startup, sizeof(startup));
    clear_bytes(&process, sizeof(process));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = output;
    startup.hStdError = output;
    launched = CreateProcessA("D:\\TLSOFF.EXE", NULL, NULL, NULL, TRUE, 0,
                              NULL, "C:\\ZUKUQA", &startup, &process);
    launch_error = launched ? 0 : GetLastError();
    record(log, "launched", launched);
    if (launched) {
        waited = WaitForSingleObject(process.hProcess, 60000);
        record(log, "wait", waited);
        if (waited == WAIT_TIMEOUT) {
            TerminateProcess(process.hProcess, 124);
            WaitForSingleObject(process.hProcess, 5000);
        }
        record(log, "exit.valid", GetExitCodeProcess(process.hProcess, &code));
        record(log, "exit.code", code);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    } else {
        record(log, "launch.error", launch_error);
    }
    CloseHandle(input);
    CloseHandle(output);
    CloseHandle(log);
    ExitProcess(launched && code == 0 ? 0 : 1);
}
