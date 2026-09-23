#include "win32_view.h"
static HINSTANCE module;
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH)
    module = instance;
  return TRUE;
}
/* Deliberately not IEWebKitGetEngineV1: this library has no complete browser,
   JavaScriptCore, TLS transport or origin authority. */
extern "C" __declspec(dllexport) HWND WINAPI IEWKSubsetCreate(
    HWND parent, iewk::SubsetLinkCallback callback, void *context) {
  try {
    return iewk::create_subset_child(parent, module, callback, context);
  } catch (...) {
    return NULL;
  }
}
extern "C" __declspec(dllexport) BOOL WINAPI IEWKSubsetAppend(
    HWND child, const char *utf8, size_t length, BOOL final_chunk) {
  return iewk::append_subset_html(child, utf8, length, !!final_chunk);
}
extern "C" __declspec(dllexport) void WINAPI IEWKSubsetReset(HWND child) {
  iewk::reset_subset_html(child);
}
/* Call outside DllMain, after all child windows have been destroyed and before
   releasing the last library reference. Registered DLL classes outlive unload.
 */
extern "C" __declspec(dllexport) BOOL WINAPI IEWKSubsetPrepareUnload() {
  return UnregisterClassA("IEWebkitSubsetChildV1", module) ||
         GetLastError() == ERROR_CLASS_DOES_NOT_EXIST;
}
