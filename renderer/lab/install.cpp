/* Explicit offline lab installation + launch in real Internet Explorer. */
#include "log.h"
#include <cstring>
#include <exdisp.h>
#include <ole2.h>
#include <windows.h>
typedef HRESULT(WINAPI *Registration)();
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR arguments, int) {
  const char *target = "C:\\IEWKSUB\\SUBHOST.DLL";
  bool remove = arguments && !_stricmp(arguments, "/remove");
  if (!remove) {
    char source[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, source, sizeof source);
    if (!n || n >= sizeof source)
      return 1;
    char *base = std::strrchr(source, '\\');
    if (!base || static_cast<size_t>(base - source) + 32 >= sizeof source)
      return 2;
    CreateDirectoryA("C:\\IEWKSUB", NULL);
    const char *files[] = {"SUBHOST.DLL", "SUBSET.DLL", "DEMO.IWK"};
    const char *destinations[] = {target, "C:\\IEWKSUB\\SUBSET.DLL",
                                  "C:\\IEWKSUB\\DEMO.IWKSUBSET"};
    for (int i = 0; i < 3; i++) {
      std::strcpy(base + 1, files[i]);
      DWORD attributes = GetFileAttributesA(destinations[i]);
      if (attributes != INVALID_FILE_ATTRIBUTES &&
          (attributes & FILE_ATTRIBUTE_READONLY) &&
          !SetFileAttributesA(destinations[i],
                              attributes & ~FILE_ATTRIBUTE_READONLY)) {
        lab_log("install.clear_readonly_error", GetLastError());
        return 3;
      }
      if (!CopyFileA(source, destinations[i], FALSE)) {
        lab_log("install.copy_error", GetLastError());
        return 3;
      }
    }
  }
  HMODULE library = LoadLibraryA(target);
  if (!library) {
    lab_log("install.dll_error", GetLastError());
    return 4;
  }
  FARPROC address = GetProcAddress(library, remove ? "DllUnregisterServer"
                                                   : "DllRegisterServer");
  Registration change = NULL;
  std::memcpy(&change, &address, sizeof change);
  HRESULT hr = change ? change() : E_NOINTERFACE;
  lab_log(remove ? "install.removed" : "install.register", hr);
  FreeLibrary(library);
  if (FAILED(hr))
    return 5;
  if (remove)
    return 0;
  hr = OleInitialize(NULL);
  if (FAILED(hr)) {
    lab_log("install.ole", hr);
    return 6;
  }
  CLSID file_class;
  HRESULT class_hr = GetClassFile(L"C:\\IEWKSUB\\DEMO.IWKSUBSET", &file_class);
  lab_log("install.file_class", class_hr);
  IWebBrowser2 *browser = NULL;
  hr = CoCreateInstance(CLSID_InternetExplorer, NULL, CLSCTX_LOCAL_SERVER,
                        IID_IWebBrowser2, reinterpret_cast<void **>(&browser));
  lab_log("install.IE_create", hr);
  if (SUCCEEDED(hr)) {
    VARIANT uri;
    VARIANT missing;
    VariantInit(&uri);
    VariantInit(&missing);
    uri.vt = VT_BSTR;
    uri.bstrVal = SysAllocString(L"file:///C:/IEWKSUB/DEMO.IWKSUBSET");
    // IE 5.5's out-of-process proxy rejects null optional VARIANT pointers.
    missing.vt = VT_ERROR;
    missing.scode = DISP_E_PARAMNOTFOUND;
    hr = uri.bstrVal ? browser->Navigate2(&uri, &missing, &missing,
                                          &missing, &missing)
                     : E_OUTOFMEMORY;
    lab_log("install.IE_navigate", hr);
    browser->put_Visible(VARIANT_TRUE);
    VariantClear(&uri);
    browser->Release();
  }
  OleUninitialize();
  return SUCCEEDED(hr) ? 0 : 7;
}
