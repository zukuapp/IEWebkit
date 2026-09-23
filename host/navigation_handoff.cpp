#include "navigation_handoff.h"
#include "engine.h"
#include <cstring>
#include <cwchar>
#include <new>

const CLSID IEWK_DOCUMENT_CLASS = {
    0x9b07cc1a,
    0xe5d2,
    0x434c,
    {0x95, 0xe8, 0xdf, 0x2f, 0xa0, 0x69, 0x4a, 0x13}};
static const CLSID HANDOFF_CLASS = {
    0x9b5ae971,
    0x6421,
    0x4978,
    {0xb4, 0x61, 0xdf, 0x1d, 0x31, 0x53, 0xea, 0xc0}};
static const WCHAR DOCUMENT_ID[] = L"{9B07CC1A-E5D2-434C-95E8-DF2FA0694A13}";
static LONG active_attachment, live_objects;
LONG iewk_handoff_live_objects() {
  return InterlockedCompareExchange(&live_objects, 0, 0);
}

class HandoffFactory;
class HandoffProtocol : public IInternetProtocol, public IInternetProtocolInfo {
  LONG refs;
  HandoffFactory *factory;
  IInternetProtocol *original;
  bool started, delegated, aborted;
  HRESULT info(IInternetProtocolInfo **out) {
    return original ? original->QueryInterface(IID_IInternetProtocolInfo,
                                               reinterpret_cast<void **>(out))
                    : E_NOINTERFACE;
  }

public:
  explicit HandoffProtocol(HandoffFactory *owner);
  virtual ~HandoffProtocol();
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) {
    if (!out)
      return E_POINTER;
    *out = NULL;
    if (IsEqualIID(id, IID_IUnknown) || IsEqualIID(id, IID_IInternetProtocol) ||
        IsEqualIID(id, IID_IInternetProtocolRoot))
      *out = static_cast<IInternetProtocol *>(this);
    else if (IsEqualIID(id, IID_IInternetProtocolInfo))
      *out = static_cast<IInternetProtocolInfo *>(this);
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
  HRESULT STDMETHODCALLTYPE Start(LPCWSTR, IInternetProtocolSink *,
                                  IInternetBindInfo *, DWORD, HANDLE_PTR);
  HRESULT STDMETHODCALLTYPE Continue(PROTOCOLDATA *p) {
    return delegated && original ? original->Continue(p) : S_OK;
  }
  HRESULT STDMETHODCALLTYPE Abort(HRESULT reason, DWORD options) {
    aborted = true;
    return delegated && original ? original->Abort(reason, options) : S_OK;
  }
  HRESULT STDMETHODCALLTYPE Terminate(DWORD options) {
    aborted = true;
    return delegated && original ? original->Terminate(options) : S_OK;
  }
  HRESULT STDMETHODCALLTYPE Suspend() {
    return delegated && original ? original->Suspend() : E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE Resume() {
    return delegated && original ? original->Resume() : E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE Read(void *out, ULONG count, ULONG *read) {
    if (delegated && original)
      return original->Read(out, count, read);
    if (!read || (!out && count))
      return E_POINTER;
    *read = 0;
    return aborted ? E_ABORT : S_FALSE;
  }
  HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER move, DWORD origin,
                                 ULARGE_INTEGER *next) {
    return delegated && original ? original->Seek(move, origin, next)
                                 : E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE LockRequest(DWORD options) {
    if (delegated && original)
      return original->LockRequest(options);
    AddRef();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE UnlockRequest() {
    if (delegated && original)
      return original->UnlockRequest();
    Release();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE ParseUrl(LPCWSTR url, PARSEACTION action,
                                     DWORD flags, LPWSTR out, DWORD size,
                                     DWORD *used, DWORD reserved) {
    IInternetProtocolInfo *p = NULL;
    HRESULT hr = info(&p);
    if (FAILED(hr))
      return INET_E_DEFAULT_ACTION;
    hr = p->ParseUrl(url, action, flags, out, size, used, reserved);
    p->Release();
    return hr;
  }
  HRESULT STDMETHODCALLTYPE CombineUrl(LPCWSTR base, LPCWSTR relative,
                                       DWORD flags, LPWSTR out, DWORD size,
                                       DWORD *used, DWORD reserved) {
    IInternetProtocolInfo *p = NULL;
    HRESULT hr = info(&p);
    if (FAILED(hr))
      return INET_E_DEFAULT_ACTION;
    hr = p->CombineUrl(base, relative, flags, out, size, used, reserved);
    p->Release();
    return hr;
  }
  HRESULT STDMETHODCALLTYPE CompareUrl(LPCWSTR first, LPCWSTR second,
                                       DWORD flags) {
    IInternetProtocolInfo *p = NULL;
    HRESULT hr = info(&p);
    if (FAILED(hr))
      return INET_E_DEFAULT_ACTION;
    hr = p->CompareUrl(first, second, flags);
    p->Release();
    return hr;
  }
  HRESULT STDMETHODCALLTYPE QueryInfo(LPCWSTR url, QUERYOPTION option,
                                      DWORD flags, LPVOID out, DWORD size,
                                      DWORD *used, DWORD reserved) {
    IInternetProtocolInfo *p = NULL;
    HRESULT hr = info(&p);
    if (FAILED(hr))
      return INET_E_DEFAULT_ACTION;
    hr = p->QueryInfo(url, option, flags, out, size, used, reserved);
    p->Release();
    return hr;
  }
};

class HandoffFactory : public IClassFactory {
  LONG refs;

public:
  DWORD thread, armed_at;
  WCHAR pending[IEWK_URL_LIMIT + 1];
  bool attached;
  HandoffFactory()
      : refs(1), thread(GetCurrentThreadId()), armed_at(0), attached(false) {
    pending[0] = 0;
    InterlockedIncrement(&live_objects);
  }
  virtual ~HandoffFactory() { InterlockedDecrement(&live_objects); }
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
    HandoffProtocol *p = new (std::nothrow) HandoffProtocol(this);
    if (!p)
      return E_OUTOFMEMORY;
    HRESULT hr = p->QueryInterface(id, out);
    p->Release();
    return hr;
  }
  HRESULT STDMETHODCALLTYPE LockServer(BOOL) { return E_NOTIMPL; }
  bool matches(LPCWSTR url) {
    if (thread != GetCurrentThreadId())
      return false;
    if (GetTickCount() - armed_at >= 10000)
      pending[0] = 0;
    return attached && thread == GetCurrentThreadId() && pending[0] &&
           GetTickCount() - armed_at < 10000 && !std::wcscmp(pending, url);
  }
};
HandoffProtocol::HandoffProtocol(HandoffFactory *owner)
    : refs(1), factory(owner), original(NULL), started(false), delegated(false),
      aborted(false) {
  factory->AddRef();
  /* Explicit system CLSID avoids recursively re-entering the namespace map. */
  CoCreateInstance(CLSID_HttpSProtocol, NULL, CLSCTX_INPROC_SERVER,
                   IID_IInternetProtocol, reinterpret_cast<void **>(&original));
}
HandoffProtocol::~HandoffProtocol() {
  if (original)
    original->Release();
  factory->Release();
}
HRESULT HandoffProtocol::Start(LPCWSTR url, IInternetProtocolSink *sink,
                               IInternetBindInfo *bind, DWORD flags,
                               HANDLE_PTR reserved) {
  if (!url || !sink || !bind)
    return E_INVALIDARG;
  if (started)
    return E_UNEXPECTED;
  started = true;
  BINDINFO info;
  DWORD bind_flags = 0;
  std::memset(&info, 0, sizeof info);
  info.cbSize = sizeof info;
  bool eligible = factory->matches(url) && !(flags & PI_PARSE_URL);
  if (eligible) {
    HRESULT hr = bind->GetBindInfo(&bind_flags, &info);
    eligible = SUCCEEDED(hr) && info.dwBindVerb == BINDVERB_GET &&
               info.cbstgmedData == 0 && info.stgmedData.tymed == TYMED_NULL;
    ReleaseBindInfo(&info);
  }
  if (!eligible) {
    delegated = true;
    return original ? original->Start(url, sink, bind, flags, reserved)
                    : INET_E_UNKNOWN_PROTOCOL;
  }
  factory->pending[0] = 0; /* Consume before callbacks can re-enter URLMon. */
  AddRef();
  sink->AddRef();
  HRESULT hr =
      sink->ReportProgress(BINDSTATUS_CLSIDCANINSTANTIATE, DOCUMENT_ID);
  if (SUCCEEDED(hr) && !aborted)
    hr = sink->ReportProgress(BINDSTATUS_MIMETYPEAVAILABLE,
                              L"application/x-iewebkit-document");
  if (SUCCEEDED(hr) && !aborted)
    hr = sink->ReportData(BSCF_FIRSTDATANOTIFICATION |
                              BSCF_LASTDATANOTIFICATION |
                              BSCF_DATAFULLYAVAILABLE,
                          0, 0);
  if (!aborted)
    sink->ReportResult(hr, 0, NULL);
  sink->Release();
  Release();
  return hr;
}
HRESULT iewk_handoff_attach(IClassFactory **out) {
  if (!out)
    return E_POINTER;
  *out = NULL;
  if (InterlockedCompareExchange(&active_attachment, 1, 0))
    return HRESULT_FROM_WIN32(ERROR_BUSY);
  HandoffFactory *factory = new (std::nothrow) HandoffFactory;
  if (!factory) {
    InterlockedExchange(&active_attachment, 0);
    return E_OUTOFMEMORY;
  }
  IInternetSession *session = NULL;
  HRESULT hr = CoInternetGetSession(0, &session, 0);
  if (SUCCEEDED(hr)) {
    hr = session->RegisterNameSpace(factory, HANDOFF_CLASS, L"https", 0, NULL,
                                    0);
    session->Release();
  }
  if (FAILED(hr)) {
    factory->Release();
    InterlockedExchange(&active_attachment, 0);
    return hr;
  }
  factory->attached = true;
  *out = factory;
  return S_OK;
}
HRESULT iewk_handoff_arm_get(IClassFactory *opaque, LPCWSTR url) {
  if (!opaque || !url)
    return E_INVALIDARG;
  HandoffFactory *factory = static_cast<HandoffFactory *>(opaque);
  if (factory->thread != GetCurrentThreadId() || !factory->attached)
    return RPC_E_WRONG_THREAD;
  factory->pending[0] = 0;
  char ascii[IEWK_URL_LIMIT + 1];
  size_t length = 0;
  while (url[length] && length < IEWK_URL_LIMIT) {
    if (url[length] >= 128)
      return E_INVALIDARG;
    ascii[length] = static_cast<char>(url[length]);
    length++;
  }
  if (url[length] || length < 8 || std::memcmp(ascii, "https://", 8) ||
      !iewk_web_url(ascii, length))
    return E_INVALIDARG;
  IMoniker *moniker = NULL;
  IBindCtx *context = NULL;
  LPOLESTR canonical = NULL;
  HRESULT hr = CreateBindCtx(0, &context);
  if (SUCCEEDED(hr))
    hr = CreateURLMoniker(NULL, url, &moniker);
  if (SUCCEEDED(hr))
    hr = moniker->GetDisplayName(context, NULL, &canonical);
  if (SUCCEEDED(hr) && canonical && std::wcslen(canonical) <= IEWK_URL_LIMIT) {
    std::wcscpy(factory->pending, canonical);
    factory->armed_at = GetTickCount();
  } else if (SUCCEEDED(hr))
    hr = E_INVALIDARG;
  if (canonical)
    CoTaskMemFree(canonical);
  if (moniker)
    moniker->Release();
  if (context)
    context->Release();
  return hr;
}
HRESULT iewk_handoff_detach(IClassFactory *opaque) {
  if (!opaque)
    return E_INVALIDARG;
  HandoffFactory *factory = static_cast<HandoffFactory *>(opaque);
  if (factory->thread != GetCurrentThreadId())
    return RPC_E_WRONG_THREAD;
  IInternetSession *session = NULL;
  HRESULT hr = CoInternetGetSession(0, &session, 0);
  if (SUCCEEDED(hr)) {
    hr = session->UnregisterNameSpace(factory, L"https");
    session->Release();
  }
  if (SUCCEEDED(hr)) {
    factory->attached = false;
    factory->pending[0] = 0;
    factory->Release();
    InterlockedExchange(&active_attachment, 0);
  }
  return hr;
}
HRESULT iewk_handoff_cancel(IClassFactory *opaque) {
  if (!opaque)
    return E_INVALIDARG;
  HandoffFactory *factory = static_cast<HandoffFactory *>(opaque);
  if (factory->thread != GetCurrentThreadId())
    return RPC_E_WRONG_THREAD;
  factory->pending[0] = 0;
  return S_OK;
}
