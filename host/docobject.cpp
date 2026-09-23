/* Site-neutral Active Document host. The URL moniker is the browser binding;
   an installed WebKit provider owns the document, scripting, TLS and pixels. */
#include "engine_loader.h"
#include <cstdio>
#include <cstring>
#include <docobj.h>
#include <exdisp.h>
#include <new>
#include <ole2.h>
#include <servprov.h>
#include <shlguid.h>
#include <urlmon.h>
#include <windows.h>
static HINSTANCE module_handle;
static LONG objects, locks;
static const CLSID CLSID_IEWKDocument = {
    0x9b07cc1a,
    0xe5d2,
    0x434c,
    {0x95, 0xe8, 0xdf, 0x2f, 0xa0, 0x69, 0x4a, 0x13}};
static const char *CLASS_ID = "{9B07CC1A-E5D2-434C-95E8-DF2FA0694A13}";
class Document : public IOleObject,
                 public IOleDocument,
                 public IOleDocumentView,
                 public IOleInPlaceObject,
                 public IOleInPlaceActiveObject,
                 public IPersistMoniker,
                 public IOleCommandTarget {
  LONG refs;
  IOleClientSite *site;
  IOleInPlaceSite *place;
  IMoniker *moniker;
  HWND window;
  RECT rectangle;
  IEWKLoadedEngine engine;
  IEWKView *view;
  IEWKHostV1 host;
  char url[IEWK_URL_LIMIT + 1];
  uint32_t navigation;
  void close_view() {
    if (view) {
      engine.api->destroy(view);
      view = NULL;
    }
    if (window) {
      DestroyWindow(window);
      window = NULL;
    }
  }
  int navigate_engine() {
    return view && url[0]
               ? engine.api->navigate(view, ++navigation, url, std::strlen(url),
                                      "GET", 3, NULL, 0)
               : IEWK_MISSING_ENGINE;
  }
  static void IEWK_CALL committed(void *context, uint32_t id, const char *value,
                                  size_t length, const char *, size_t,
                                  uint32_t) {
    Document *self = static_cast<Document *>(context);
    if (id != self->navigation || !iewk_web_url(value, length) ||
        length >= sizeof self->url)
      return;
    std::memcpy(self->url, value, length);
    self->url[length] = 0;
    WCHAR wide[IEWK_URL_LIMIT + 1];
    int n = MultiByteToWideChar(CP_UTF8, 0, value, static_cast<int>(length),
                                wide, IEWK_URL_LIMIT);
    if (n <= 0)
      return;
    wide[n] = 0;
    IMoniker *current = NULL;
    if (SUCCEEDED(CreateURLMoniker(NULL, wide, &current))) {
      if (self->moniker)
        self->moniker->Release();
      self->moniker = current;
    }
    /* IE-specific travel-log notification belongs to the selected adapter.
       Never issue SET_SSL_LOCK from a requested URL or synthesize origin data.
     */
  }
  static void IEWK_CALL title(void *context, const char *text, size_t length) {
    Document *self = static_cast<Document *>(context);
    if (!self->site || length > 1024)
      return;
    WCHAR wide[1025];
    int n = MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(length),
                                wide, 1024);
    if (n <= 0)
      return;
    wide[n] = 0;
    IOleCommandTarget *commands = NULL;
    if (SUCCEEDED(self->site->QueryInterface(
            IID_IOleCommandTarget, reinterpret_cast<void **>(&commands)))) {
      VARIANT value;
      VariantInit(&value);
      value.vt = VT_BSTR;
      value.bstrVal = SysAllocStringLen(wide, n);
      if (value.bstrVal)
        commands->Exec(NULL, OLECMDID_SETTITLE, OLECMDEXECOPT_DONTPROMPTUSER,
                       &value, NULL);
      VariantClear(&value);
      commands->Release();
    }
  }
  static void IEWK_CALL failed(void *, uint32_t, int, const char *, size_t) {}
  static void IEWK_CALL request(void *context, const char *target,
                                size_t length, const char *method,
                                size_t method_length, const void *body,
                                size_t body_length) {
    Document *self = static_cast<Document *>(context);
    if (!self->site || !iewk_web_url(target, length) || body_length > 1048576)
      return;
    if (!((method_length == 3 && !std::memcmp(method, "GET", 3) &&
           body_length == 0) ||
          (method_length == 4 && !std::memcmp(method, "POST", 4))))
      return;
    IServiceProvider *provider = NULL;
    IWebBrowser2 *browser = NULL;
    if (FAILED(self->site->QueryInterface(
            IID_IServiceProvider, reinterpret_cast<void **>(&provider))))
      return;
    HRESULT hr = provider->QueryService(SID_SWebBrowserApp, IID_IWebBrowser2,
                                        reinterpret_cast<void **>(&browser));
    provider->Release();
    if (FAILED(hr) || !browser)
      return;
    WCHAR wide[IEWK_URL_LIMIT + 1];
    int n = MultiByteToWideChar(CP_UTF8, 0, target, static_cast<int>(length),
                                wide, IEWK_URL_LIMIT);
    if (n <= 0) {
      browser->Release();
      return;
    }
    wide[n] = 0;
    VARIANT destination, post;
    VariantInit(&destination);
    VariantInit(&post);
    destination.vt = VT_BSTR;
    destination.bstrVal = SysAllocStringLen(wide, n);
    if (method_length == 4) {
      post.vt = VT_ARRAY | VT_UI1;
      post.parray =
          SafeArrayCreateVector(VT_UI1, 0, static_cast<ULONG>(body_length));
      if (!post.parray) {
        VariantClear(&destination);
        browser->Release();
        return;
      }
      void *bytes = NULL;
      if (SUCCEEDED(SafeArrayAccessData(post.parray, &bytes))) {
        if (body_length)
          std::memcpy(bytes, body, body_length);
        SafeArrayUnaccessData(post.parray);
      } else {
        VariantClear(&post);
        VariantClear(&destination);
        browser->Release();
        return;
      }
    }
    if (destination.bstrVal)
      browser->Navigate2(&destination, NULL, NULL, &post, NULL);
    VariantClear(&post);
    VariantClear(&destination);
    browser->Release();
  }
  HRESULT show() {
    if (!place)
      return E_UNEXPECTED;
    HWND parent = NULL;
    HRESULT hr = place->CanInPlaceActivate();
    if (FAILED(hr))
      return hr;
    if (FAILED(place->GetWindow(&parent)) || !parent)
      return E_FAIL;
    if (!window) {
      if (!engine.library &&
          iewk_load_engine(module_handle, &engine) != IEWK_OK)
        return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
      window = CreateWindowA(
          "STATIC", "", WS_CHILD | WS_CLIPCHILDREN, rectangle.left,
          rectangle.top, rectangle.right - rectangle.left,
          rectangle.bottom - rectangle.top, parent, NULL, module_handle, NULL);
      if (!window)
        return E_FAIL;
      if (engine.api->create(&host, window, &view) != IEWK_OK || !view) {
        close_view();
        return E_FAIL;
      }
      if (url[0] && navigate_engine() != IEWK_OK) {
        close_view();
        return E_FAIL;
      }
      place->OnInPlaceActivate();
    }
    engine.api->resize(view, 0, 0, rectangle.right - rectangle.left,
                       rectangle.bottom - rectangle.top);
    ShowWindow(window, SW_SHOW);
    return S_OK;
  }

public:
  Document()
      : refs(1), site(NULL), place(NULL), moniker(NULL), window(NULL),
        view(NULL), navigation(0) {
    std::memset(&engine, 0, sizeof engine);
    std::memset(&host, 0, sizeof host);
    url[0] = 0;
    ::SetRect(&rectangle, 0, 0, 640, 480);
    host.size = sizeof host;
    host.abi = IEWK_ABI_V1;
    host.context = this;
    host.committed = committed;
    host.title = title;
    host.failed = failed;
    host.navigate_requested = request;
    InterlockedIncrement(&objects);
  }
  virtual ~Document() {
    close_view();
    iewk_unload_engine(&engine);
    if (moniker)
      moniker->Release();
    if (place)
      place->Release();
    if (site)
      site->Release();
    InterlockedDecrement(&objects);
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) {
    if (!out)
      return E_POINTER;
    *out = NULL;
    if (IsEqualIID(id, IID_IUnknown) || IsEqualIID(id, IID_IOleObject))
      *out = static_cast<IOleObject *>(this);
    else if (IsEqualIID(id, IID_IOleDocument))
      *out = static_cast<IOleDocument *>(this);
    else if (IsEqualIID(id, IID_IOleDocumentView))
      *out = static_cast<IOleDocumentView *>(this);
    else if (IsEqualIID(id, IID_IOleWindow) ||
             IsEqualIID(id, IID_IOleInPlaceObject))
      *out = static_cast<IOleInPlaceObject *>(this);
    else if (IsEqualIID(id, IID_IOleInPlaceActiveObject))
      *out = static_cast<IOleInPlaceActiveObject *>(this);
    else if (IsEqualIID(id, IID_IPersist) ||
             IsEqualIID(id, IID_IPersistMoniker))
      *out = static_cast<IPersistMoniker *>(this);
    else if (IsEqualIID(id, IID_IOleCommandTarget))
      *out = static_cast<IOleCommandTarget *>(this);
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
  HRESULT STDMETHODCALLTYPE SetClientSite(IOleClientSite *next) {
    if (next)
      next->AddRef();
    close_view();
    if (site)
      site->Release();
    site = next;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetClientSite(IOleClientSite **out) {
    if (!out)
      return E_POINTER;
    *out = site;
    if (site)
      site->AddRef();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE SetHostNames(LPCOLESTR, LPCOLESTR) { return S_OK; }
  HRESULT STDMETHODCALLTYPE Close(DWORD) {
    close_view();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE SetMoniker(DWORD, IMoniker *next) {
    if (next)
      next->AddRef();
    if (moniker)
      moniker->Release();
    moniker = next;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetMoniker(DWORD, DWORD, IMoniker **out) {
    return GetCurMoniker(out);
  }
  HRESULT STDMETHODCALLTYPE InitFromData(IDataObject *, BOOL, DWORD) {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetClipboardData(DWORD, IDataObject **out) {
    if (out)
      *out = NULL;
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE DoVerb(LONG verb, LPMSG, IOleClientSite *client,
                                   LONG, HWND, LPCRECT rect) {
    if (client && client != site)
      SetClientSite(client);
    if (rect)
      rectangle = *rect;
    if (verb == OLEIVERB_HIDE)
      return Show(FALSE);
    if (verb != OLEIVERB_SHOW && verb != OLEIVERB_PRIMARY &&
        verb != OLEIVERB_INPLACEACTIVATE && verb != OLEIVERB_UIACTIVATE)
      return OLEOBJ_S_INVALIDVERB;
    if (!place && site)
      site->QueryInterface(IID_IOleInPlaceSite,
                           reinterpret_cast<void **>(&place));
    return show();
  }
  HRESULT STDMETHODCALLTYPE EnumVerbs(IEnumOLEVERB **out) {
    if (out)
      *out = NULL;
    return OLE_S_USEREG;
  }
  HRESULT STDMETHODCALLTYPE Update() { return S_OK; }
  HRESULT STDMETHODCALLTYPE IsUpToDate() { return S_OK; }
  HRESULT STDMETHODCALLTYPE GetUserClassID(CLSID *out) {
    return GetClassID(out);
  }
  HRESULT STDMETHODCALLTYPE GetUserType(DWORD, LPOLESTR *out) {
    if (out)
      *out = NULL;
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE SetExtent(DWORD, SIZEL *) { return S_OK; }
  HRESULT STDMETHODCALLTYPE GetExtent(DWORD, SIZEL *out) {
    if (!out)
      return E_POINTER;
    out->cx = 16933;
    out->cy = 12700;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Advise(IAdviseSink *, DWORD *cookie) {
    if (cookie)
      *cookie = 0;
    return OLE_E_ADVISENOTSUPPORTED;
  }
  HRESULT STDMETHODCALLTYPE Unadvise(DWORD) { return OLE_E_NOCONNECTION; }
  HRESULT STDMETHODCALLTYPE EnumAdvise(IEnumSTATDATA **out) {
    if (out)
      *out = NULL;
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetMiscStatus(DWORD, DWORD *out) {
    if (!out)
      return E_POINTER;
    *out = OLEMISC_INSIDEOUT | OLEMISC_ACTIVATEWHENVISIBLE |
           OLEMISC_SETCLIENTSITEFIRST;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE SetColorScheme(LOGPALETTE *) { return S_OK; }
  HRESULT STDMETHODCALLTYPE CreateView(IOleInPlaceSite *value, IStream *, DWORD,
                                       IOleDocumentView **out) {
    if (!out)
      return E_POINTER;
    *out = NULL;
    SetInPlaceSite(value);
    *out = static_cast<IOleDocumentView *>(this);
    AddRef();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetDocMiscStatus(DWORD *out) {
    if (!out)
      return E_POINTER;
    *out = DOCMISC_CANTOPENEDIT | DOCMISC_NOFILESUPPORT;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE EnumViews(IEnumOleDocumentViews **enumerator,
                                      IOleDocumentView **out) {
    if (enumerator)
      *enumerator = NULL;
    if (!out)
      return E_POINTER;
    *out = static_cast<IOleDocumentView *>(this);
    AddRef();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE SetInPlaceSite(IOleInPlaceSite *next) {
    if (next)
      next->AddRef();
    if (place)
      place->Release();
    place = next;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetInPlaceSite(IOleInPlaceSite **out) {
    if (!out)
      return E_POINTER;
    *out = place;
    if (place)
      place->AddRef();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetDocument(IUnknown **out) {
    return QueryInterface(IID_IUnknown, reinterpret_cast<void **>(out));
  }
  HRESULT STDMETHODCALLTYPE SetRect(LPRECT value) {
    if (!value)
      return E_POINTER;
    rectangle = *value;
    if (window) {
      MoveWindow(window, rectangle.left, rectangle.top,
                 rectangle.right - rectangle.left,
                 rectangle.bottom - rectangle.top, TRUE);
      engine.api->resize(view, 0, 0, rectangle.right - rectangle.left,
                         rectangle.bottom - rectangle.top);
    }
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetRect(LPRECT out) {
    if (!out)
      return E_POINTER;
    *out = rectangle;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE SetRectComplex(LPRECT value, LPRECT, LPRECT,
                                           LPRECT) {
    return SetRect(value);
  }
  HRESULT STDMETHODCALLTYPE Show(BOOL visible) {
    if (visible)
      return show();
    if (window)
      ShowWindow(window, SW_HIDE);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE UIActivate(BOOL activate) {
    if (!activate) {
      if (view)
        engine.api->focus(view, 0);
      return S_OK;
    }
    HRESULT hr = show();
    if (SUCCEEDED(hr)) {
      place->OnUIActivate();
      engine.api->focus(view, 1);
    }
    return hr;
  }
  HRESULT STDMETHODCALLTYPE Open() { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE CloseView(DWORD) {
    close_view();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE SaveViewState(IStream *) { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE ApplyViewState(IStream *) { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE Clone(IOleInPlaceSite *, IOleDocumentView **out) {
    if (out)
      *out = NULL;
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetWindow(HWND *out) {
    if (!out)
      return E_POINTER;
    *out = window;
    return window ? S_OK : E_FAIL;
  }
  HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL) { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE InPlaceDeactivate() {
    close_view();
    return place ? place->OnInPlaceDeactivate() : S_OK;
  }
  HRESULT STDMETHODCALLTYPE UIDeactivate() {
    if (view)
      engine.api->focus(view, 0);
    return place ? place->OnUIDeactivate(FALSE) : S_OK;
  }
  HRESULT STDMETHODCALLTYPE SetObjectRects(LPCRECT rect, LPCRECT) {
    if (!rect)
      return E_POINTER;
    RECT copy = *rect;
    return SetRect(&copy);
  }
  HRESULT STDMETHODCALLTYPE ReactivateAndUndo() { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE TranslateAccelerator(LPMSG) { return S_FALSE; }
  HRESULT STDMETHODCALLTYPE OnFrameWindowActivate(BOOL) { return S_OK; }
  HRESULT STDMETHODCALLTYPE OnDocWindowActivate(BOOL) { return S_OK; }
  HRESULT STDMETHODCALLTYPE ResizeBorder(LPCRECT, IOleInPlaceUIWindow *, BOOL) {
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE EnableModeless(BOOL) { return S_OK; }
  HRESULT STDMETHODCALLTYPE GetClassID(CLSID *out) {
    if (!out)
      return E_POINTER;
    *out = CLSID_IEWKDocument;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE IsDirty() { return S_FALSE; }
  HRESULT STDMETHODCALLTYPE Load(BOOL, IMoniker *source, IBindCtx *context,
                                 DWORD) {
    if (!source || !context)
      return E_INVALIDARG;
    LPOLESTR name = NULL;
    HRESULT hr = source->GetDisplayName(context, NULL, &name);
    if (FAILED(hr) || !name)
      return E_INVALIDARG;
    int n =
        WideCharToMultiByte(CP_UTF8, 0, name, -1, url, sizeof url, NULL, NULL);
    CoTaskMemFree(name);
    if (n <= 0 || !iewk_web_url(url, static_cast<size_t>(n - 1))) {
      url[0] = 0;
      return E_ACCESSDENIED;
    }
    SetMoniker(0, source);
    return view ? (navigate_engine() == IEWK_OK ? S_OK : E_FAIL) : S_OK;
  }
  HRESULT STDMETHODCALLTYPE Save(IMoniker *, IBindCtx *, BOOL) {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE SaveCompleted(IMoniker *, IBindCtx *) {
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetCurMoniker(IMoniker **out) {
    if (!out)
      return E_POINTER;
    *out = moniker;
    if (moniker)
      moniker->AddRef();
    return moniker ? S_OK : E_UNEXPECTED;
  }
  HRESULT STDMETHODCALLTYPE QueryStatus(const GUID *group, ULONG count,
                                        OLECMD *commands, OLECMDTEXT *) {
    if (group)
      return OLECMDERR_E_UNKNOWNGROUP;
    if (!commands)
      return E_POINTER;
    for (ULONG i = 0; i < count; i++)
      commands[i].cmdf = (commands[i].cmdID == OLECMDID_STOP ||
                          commands[i].cmdID == OLECMDID_REFRESH)
                             ? OLECMDF_SUPPORTED | (view ? OLECMDF_ENABLED : 0)
                             : 0;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Exec(const GUID *group, DWORD command, DWORD,
                                 VARIANT *, VARIANT *) {
    if (group)
      return OLECMDERR_E_UNKNOWNGROUP;
    if (command != OLECMDID_STOP && command != OLECMDID_REFRESH)
      return OLECMDERR_E_NOTSUPPORTED;
    return view && engine.api->command(view, command) == IEWK_OK ? S_OK
                                                                 : E_FAIL;
  }
};
class Factory : public IClassFactory {
  LONG refs;

public:
  Factory() : refs(1) { InterlockedIncrement(&objects); }
  virtual ~Factory() { InterlockedDecrement(&objects); }
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
    Document *doc = new (std::nothrow) Document;
    if (!doc)
      return E_OUTOFMEMORY;
    HRESULT hr = doc->QueryInterface(id, out);
    doc->Release();
    return hr;
  }
  HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) {
    if (lock)
      InterlockedIncrement(&locks);
    else
      InterlockedDecrement(&locks);
    return S_OK;
  }
};
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    module_handle = instance;
    DisableThreadLibraryCalls(instance);
  }
  return TRUE;
}
extern "C" __declspec(dllexport) HRESULT WINAPI
DllGetClassObject(REFCLSID clsid, REFIID id, void **out) {
  if (!out)
    return E_POINTER;
  *out = NULL;
  if (!IsEqualCLSID(clsid, CLSID_IEWKDocument))
    return CLASS_E_CLASSNOTAVAILABLE;
  Factory *factory = new (std::nothrow) Factory;
  if (!factory)
    return E_OUTOFMEMORY;
  HRESULT hr = factory->QueryInterface(id, out);
  factory->Release();
  return hr;
}
extern "C" __declspec(dllexport) HRESULT WINAPI DllCanUnloadNow() {
  return objects == 0 && locks == 0 ? S_OK : S_FALSE;
}
static LONG reg(const char *path, const char *name, const char *value) {
  HKEY key;
  LONG r = RegCreateKeyExA(HKEY_CLASSES_ROOT, path, 0, NULL, 0, KEY_WRITE, NULL,
                           &key, NULL);
  if (r == ERROR_SUCCESS) {
    r = RegSetValueExA(key, name, 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(value),
                       static_cast<DWORD>(std::strlen(value) + 1));
    RegCloseKey(key);
  }
  return r;
}
extern "C" __declspec(dllexport) HRESULT WINAPI DllRegisterServer() {
  char path[256], file[MAX_PATH];
  DWORD n = GetModuleFileNameA(module_handle, file, sizeof file);
  if (!n || n >= sizeof file)
    return E_FAIL;
  std::snprintf(path, sizeof path, "CLSID\\%s\\InprocServer32", CLASS_ID);
  if (reg(path, NULL, file) || reg(path, "ThreadingModel", "Apartment"))
    return E_FAIL;
  std::snprintf(path, sizeof path, "CLSID\\%s\\DocObject", CLASS_ID);
  if (reg(path, NULL, "0"))
    return E_FAIL;
  return reg("MIME\\Database\\Content Type\\application/x-iewebkit-document",
             "CLSID", CLASS_ID)
             ? E_FAIL
             : S_OK;
}
extern "C" __declspec(dllexport) HRESULT WINAPI DllUnregisterServer() {
  char path[256];
  RegDeleteKeyA(
      HKEY_CLASSES_ROOT,
      "MIME\\Database\\Content Type\\application/x-iewebkit-document");
  std::snprintf(path, sizeof path, "CLSID\\%s\\DocObject", CLASS_ID);
  RegDeleteKeyA(HKEY_CLASSES_ROOT, path);
  std::snprintf(path, sizeof path, "CLSID\\%s\\InprocServer32", CLASS_ID);
  RegDeleteKeyA(HKEY_CLASSES_ROOT, path);
  std::snprintf(path, sizeof path, "CLSID\\%s", CLASS_ID);
  RegDeleteKeyA(HKEY_CLASSES_ROOT, path);
  return S_OK;
}
