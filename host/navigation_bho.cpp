/* Opt-in IE5.5/6 development adapter. No global protocol registration. */
#include "engine_loader.h"
#include "navigation_handoff.h"
#include "navigation_trace.h"
#include <cstdio>
#include <cstring>
#include <exdisp.h>
#include <exdispid.h>
#include <new>
#include <ocidl.h>
static HINSTANCE instance;
static LONG object_count;
static const CLSID BHO_CLASS = {
    0x73926903,
    0xb304,
    0x4b92,
    {0x86, 0x04, 0x20, 0x06, 0x50, 0x47, 0xa1, 0x3a}};
static const char *BHO_ID = "{73926903-B304-4B92-8604-20065047A13A}";
static VARIANT *unbox(VARIANT *v) {
  /* IE5.5 wraps PostData in two VT_VARIANT|VT_BYREF layers. Keep traversal
     bounded so malformed/cyclic automation values cannot loop. */
  for (unsigned depth = 0; v && v->vt == (VT_VARIANT | VT_BYREF) && depth < 4;
       depth++)
    v = v->pvarVal;
  return v && v->vt == (VT_VARIANT | VT_BYREF) ? NULL : v;
}
static bool empty(VARIANT *v) {
  v = unbox(v);
  if (v && v->vt == (VT_ARRAY | VT_UI1) && v->parray &&
      SafeArrayGetDim(v->parray) == 1) {
    LONG lower = 0, upper = 0;
    return SUCCEEDED(SafeArrayGetLBound(v->parray, 1, &lower)) &&
           SUCCEEDED(SafeArrayGetUBound(v->parray, 1, &upper)) && upper < lower;
  }
  return v &&
         (v->vt == VT_EMPTY || v->vt == VT_NULL ||
          (v->vt == VT_BSTR && (!v->bstrVal || !SysStringLen(v->bstrVal))));
}
static bool same(IUnknown *a, IUnknown *b) {
  IUnknown *x = NULL, *y = NULL;
  bool equal = false;
  if (a && b &&
      SUCCEEDED(
          a->QueryInterface(IID_IUnknown, reinterpret_cast<void **>(&x))) &&
      SUCCEEDED(b->QueryInterface(IID_IUnknown, reinterpret_cast<void **>(&y))))
    equal = x == y;
  if (x)
    x->Release();
  if (y)
    y->Release();
  return equal;
}
class BrowserHelper : public IObjectWithSite, public IDispatch {
  LONG refs;
  IWebBrowser2 *browser;
  IConnectionPoint *point;
  DWORD cookie;
  IClassFactory *handoff;
  IEWKLoadedEngine engine;

public:
  BrowserHelper()
      : refs(1), browser(NULL), point(NULL), cookie(0), handoff(NULL) {
    std::memset(&engine, 0, sizeof engine);
    InterlockedIncrement(&object_count);
  }
  virtual ~BrowserHelper() {
    SetSite(NULL);
    InterlockedDecrement(&object_count);
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) {
    if (!out)
      return E_POINTER;
    *out = NULL;
    if (IsEqualIID(id, IID_IUnknown) || IsEqualIID(id, IID_IObjectWithSite))
      *out = static_cast<IObjectWithSite *>(this);
    else if (IsEqualIID(id, IID_IDispatch) ||
             IsEqualIID(id, DIID_DWebBrowserEvents2))
      *out = static_cast<IDispatch *>(this);
    if (!*out)
      return E_NOINTERFACE;
    AddRef();
    return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&refs); }
  ULONG STDMETHODCALLTYPE Release() {
    ULONG n = InterlockedDecrement(&refs);
    if (!n)
      delete this;
    return n;
  }
  HRESULT STDMETHODCALLTYPE SetSite(IUnknown *site) {
    if (point) {
      IConnectionPoint *old = point;
      point = NULL;
      old->Unadvise(cookie);
      old->Release();
      cookie = 0;
    }
    if (handoff) {
      iewk_handoff_detach(handoff);
      handoff = NULL;
    }
    if (browser) {
      browser->Release();
      browser = NULL;
    }
    iewk_unload_engine(&engine);
    if (!site)
      return S_OK;
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, sizeof path);
    const char *name = n && n < sizeof path ? std::strrchr(path, '\\') : NULL;
    if (!name || _stricmp(name + 1, "iexplore.exe"))
      return E_ACCESSDENIED;
    HRESULT hr = site->QueryInterface(IID_IWebBrowser2,
                                      reinterpret_cast<void **>(&browser));
    iewk_navigation_trace("BHO.browser", hr);
    if (FAILED(hr))
      return hr;
#ifndef IEWK_NAV_DIAGNOSTIC
    /* No interception before a real complete engine is installed. */
    if (iewk_load_engine(instance, &engine) != IEWK_OK) {
      browser->Release();
      browser = NULL;
      return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    }
#endif
    IConnectionPointContainer *container = NULL;
    hr = browser->QueryInterface(IID_IConnectionPointContainer,
                                 reinterpret_cast<void **>(&container));
    if (SUCCEEDED(hr)) {
      hr = container->FindConnectionPoint(DIID_DWebBrowserEvents2, &point);
      container->Release();
    }
    if (SUCCEEDED(hr))
      hr = iewk_handoff_attach(&handoff);
    if (SUCCEEDED(hr))
      hr = point->Advise(static_cast<IDispatch *>(this), &cookie);
    iewk_navigation_trace("BHO.attach", hr);
    if (FAILED(hr))
      SetSite(NULL);
    return hr;
  }
  HRESULT STDMETHODCALLTYPE GetSite(REFIID id, void **out) {
    if (!out)
      return E_POINTER;
    *out = NULL;
    return browser ? browser->QueryInterface(id, out) : E_FAIL;
  }
  HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *out) {
    if (!out)
      return E_POINTER;
    *out = 0;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo **out) {
    if (out)
      *out = NULL;
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR *, UINT, LCID,
                                          DISPID *) {
    return DISP_E_UNKNOWNNAME;
  }
  HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD,
                                   DISPPARAMS *args, VARIANT *, EXCEPINFO *,
                                   UINT *) {
    if (id != DISPID_BEFORENAVIGATE2)
      return S_OK;
    if (!args || args->cArgs != 7 || !browser || !handoff)
      return E_INVALIDARG;
    VARIANT *dispatch = unbox(&args->rgvarg[6]), *url = unbox(&args->rgvarg[5]);
    bool top = dispatch && dispatch->vt == VT_DISPATCH &&
               same(dispatch->pdispVal, browser);
    iewk_navigation_trace("BHO.top_navigation", top);
    iewk_navigation_trace("BHO.url_vt", url ? url->vt : 0xffff);
    VARIANT *post = unbox(&args->rgvarg[2]), *headers = unbox(&args->rgvarg[1]);
    iewk_navigation_trace("BHO.post_vt", post ? post->vt : 0xffff);
    iewk_navigation_trace("BHO.headers_vt", headers ? headers->vt : 0xffff);
    if (top)
      iewk_handoff_cancel(handoff);
    if (!top || !url || url->vt != VT_BSTR || !empty(&args->rgvarg[2]) ||
        !empty(&args->rgvarg[1]))
      return S_OK;
    HRESULT hr = iewk_handoff_arm_get(handoff, url->bstrVal);
    iewk_navigation_trace("BHO.arm_get", hr);
    /* Let IE keep its real navigation transaction and address bar. No cancel,
       local URL rewrite, synthetic security icon, or browser DOM credentials.
     */
    return S_OK;
  }
};
class HelperFactory : public IClassFactory {
  LONG refs;

public:
  HelperFactory() : refs(1) { InterlockedIncrement(&object_count); }
  virtual ~HelperFactory() { InterlockedDecrement(&object_count); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) {
    if (!out)
      return E_POINTER;
    *out = NULL;
    if (!IsEqualIID(id, IID_IUnknown) && !IsEqualIID(id, IID_IClassFactory))
      return E_NOINTERFACE;
    *out = static_cast<IClassFactory *>(this);
    AddRef();
    return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&refs); }
  ULONG STDMETHODCALLTYPE Release() {
    ULONG n = InterlockedDecrement(&refs);
    if (!n)
      delete this;
    return n;
  }
  HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *outer, REFIID id,
                                           void **out) {
    if (!out)
      return E_POINTER;
    *out = NULL;
    if (outer)
      return CLASS_E_NOAGGREGATION;
    BrowserHelper *p = new (std::nothrow) BrowserHelper;
    if (!p)
      return E_OUTOFMEMORY;
    HRESULT hr = p->QueryInterface(id, out);
    p->Release();
    return hr;
  }
  HRESULT STDMETHODCALLTYPE LockServer(BOOL) { return E_NOTIMPL; }
};
extern "C" BOOL WINAPI DllMain(HINSTANCE value, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    instance = value;
    DisableThreadLibraryCalls(value);
  }
  return TRUE;
}
extern "C" __declspec(dllexport) HRESULT WINAPI
DllGetClassObject(REFCLSID clsid, REFIID id, void **out) {
  if (!out)
    return E_POINTER;
  *out = NULL;
  if (!IsEqualCLSID(clsid, BHO_CLASS))
    return CLASS_E_CLASSNOTAVAILABLE;
  HelperFactory *p = new (std::nothrow) HelperFactory;
  if (!p)
    return E_OUTOFMEMORY;
  HRESULT hr = p->QueryInterface(id, out);
  p->Release();
  return hr;
}
extern "C" __declspec(dllexport) HRESULT WINAPI DllCanUnloadNow() {
  return object_count || iewk_handoff_live_objects() ? S_FALSE : S_OK;
}
static LONG registry(HKEY root, const char *path, const char *name,
                     const char *value) {
  HKEY key;
  LONG result =
      RegCreateKeyExA(root, path, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
  if (!result) {
    result = RegSetValueExA(key, name, 0, REG_SZ,
                            reinterpret_cast<const BYTE *>(value),
                            std::strlen(value) + 1);
    RegCloseKey(key);
  }
  return result;
}
extern "C" __declspec(dllexport) HRESULT WINAPI DllRegisterServer() {
  char file[MAX_PATH], key[256];
  DWORD n = GetModuleFileNameA(instance, file, sizeof file);
  if (!n || n >= sizeof file)
    return E_FAIL;
  std::snprintf(key, sizeof key, "CLSID\\%s\\InprocServer32", BHO_ID);
  if (registry(HKEY_CLASSES_ROOT, key, NULL, file) ||
      registry(HKEY_CLASSES_ROOT, key, "ThreadingModel", "Apartment"))
    return E_FAIL;
  std::snprintf(key, sizeof key,
                "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Browse"
                "r Helper Objects\\%s",
                BHO_ID);
  return registry(HKEY_LOCAL_MACHINE, key, NULL,
                  "IEWebkit development navigation adapter")
             ? E_FAIL
             : S_OK;
}
extern "C" __declspec(dllexport) HRESULT WINAPI DllUnregisterServer() {
  char key[256];
  std::snprintf(key, sizeof key,
                "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Browse"
                "r Helper Objects\\%s",
                BHO_ID);
  RegDeleteKeyA(HKEY_LOCAL_MACHINE, key);
  std::snprintf(key, sizeof key, "CLSID\\%s\\InprocServer32", BHO_ID);
  RegDeleteKeyA(HKEY_CLASSES_ROOT, key);
  std::snprintf(key, sizeof key, "CLSID\\%s", BHO_ID);
  RegDeleteKeyA(HKEY_CLASSES_ROOT, key);
  return S_OK;
}
