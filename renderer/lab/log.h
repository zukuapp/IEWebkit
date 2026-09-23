#ifndef IEWK_SUBSET_LAB_LOG_H
#define IEWK_SUBSET_LAB_LOG_H
#include <windows.h>
static void lab_log(const char *key, DWORD value) {
  HANDLE file = CreateFileA("C:\\SUBSET.LOG", GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file != INVALID_HANDLE_VALUE) {
    char line[160];
    DWORD written;
    wsprintfA(line, "%s=0x%08lx\r\n", key, value);
    SetFilePointer(file, 0, NULL, FILE_END);
    WriteFile(file, line, lstrlenA(line), &written, NULL);
    CloseHandle(file);
  }
}
#endif
