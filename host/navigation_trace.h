#ifndef IEWK_NAVIGATION_TRACE_H
#define IEWK_NAVIGATION_TRACE_H
#include <windows.h>
/* Explicit offline QA builds only. Never compile this into release artifacts.
 */
static inline void iewk_navigation_trace(const char *key, DWORD value) {
#ifdef IEWK_NAV_DIAGNOSTIC
  HANDLE file = CreateFileA("C:\\ZUKUQA\\IENAV.LOG", GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file != INVALID_HANDLE_VALUE) {
    char text[160];
    DWORD written;
    SetFilePointer(file, 0, NULL, FILE_END);
    wsprintfA(text, "%s=0x%08lx\r\n", key, value);
    WriteFile(file, text, lstrlenA(text), &written, NULL);
    CloseHandle(file);
  }
#else
  (void)key;
  (void)value;
#endif
}
#endif
