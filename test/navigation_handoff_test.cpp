/* Real URLMon binding to the actual host Document, without a renderer or TLS.
 */
#include "../host/docobject.cpp"
#include "../host/navigation_handoff.h"
#include <cwchar>
static HANDLE output;
static void report(const char *name, DWORD value) {
  char text[180];
  DWORD wrote;
  wsprintfA(text, "%s=0x%08lx\r\n", name, value);
  WriteFile(output, text, lstrlenA(text), &wrote, NULL);
  FlushFileBuffers(output);
}
class Callback : public IBindStatusCallback {
  LONG refs;

public:
  IUnknown *object;
  IBinding *binding;
  bool stopped;
  HRESULT result;
  Callback()
      : refs(1), object(NULL), binding(NULL), stopped(false),
        result(E_PENDING) {}
  virtual ~Callback() {
    if (object)
      object->Release();
    if (binding)
      binding->Release();
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) {
    if (!out)
      return E_POINTER;
    *out = NULL;
    if (!IsEqualIID(id, IID_IUnknown) &&
        !IsEqualIID(id, IID_IBindStatusCallback))
      return E_NOINTERFACE;
    *out = static_cast<IBindStatusCallback *>(this);
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
  HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD, IBinding *b) {
    binding = b;
    b->AddRef();
    report("OnStartBinding", 0);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetPriority(LONG *p) {
    if (!p)
      return E_POINTER;
    *p = 0;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnLowResource(DWORD) { return S_OK; }
  HRESULT STDMETHODCALLTYPE OnProgress(ULONG, ULONG, ULONG status, LPCWSTR) {
    report("OnProgress", status);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT hr, LPCWSTR) {
    result = hr;
    stopped = true;
    report("OnStopBinding", hr);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD *flags, BINDINFO *info) {
    if (!flags || !info)
      return E_POINTER;
    *flags = BINDF_ASYNCHRONOUS | BINDF_ASYNCSTORAGE | BINDF_NOWRITECACHE;
    DWORD size = info->cbSize;
    std::memset(info, 0, size);
    info->cbSize = size;
    info->dwBindVerb = BINDVERB_GET;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD, DWORD, FORMATETC *,
                                            STGMEDIUM *) {
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID, IUnknown *p) {
    if (object)
      object->Release();
    object = p;
    p->AddRef();
    report("OnObjectAvailable", 0);
    return S_OK;
  }
};
static bool bind_document(IClassFactory *handoff, LPCWSTR target) {
  HRESULT hr = iewk_handoff_arm_get(handoff, target);
  report("arm", hr);
  if (FAILED(hr))
    return false;
  Callback *callback = new Callback;
  IBindCtx *context = NULL;
  IMoniker *source = NULL;
  IUnknown *bound = NULL;
  bool passed = false;
  hr = CreateAsyncBindCtx(0, callback, NULL, &context);
  report("CreateAsyncBindCtx", hr);
  if (SUCCEEDED(hr))
    hr = CreateURLMoniker(NULL, target, &source);
  if (SUCCEEDED(hr))
    hr = source->BindToObject(context, NULL, IID_IUnknown,
                              reinterpret_cast<void **>(&bound));
  report("BindToObject", hr);
  DWORD begin = GetTickCount();
  MSG msg;
  while (!bound && !callback->object && !callback->stopped &&
         GetTickCount() - begin < 10000) {
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
    Sleep(10);
  }
  if (!bound && callback->object) {
    bound = callback->object;
    bound->AddRef();
  }
  if (bound) {
    IPersistMoniker *persist = NULL;
    IMoniker *current = NULL;
    LPOLESTR name = NULL;
    hr = bound->QueryInterface(IID_IPersistMoniker,
                               reinterpret_cast<void **>(&persist));
    report("QI.IPersistMoniker", hr);
    /* BindToObject selects the class. IE's document host must then load the
       original URL moniker; URLMon does not perform this step for the caller.
     */
    if (SUCCEEDED(hr))
      hr = persist->Load(FALSE, source, context, STGM_READ);
    report("explicit.IPersistMoniker.Load", hr);
    if (SUCCEEDED(hr))
      hr = persist->GetCurMoniker(&current);
    report("GetCurMoniker", hr);
    if (SUCCEEDED(hr))
      hr = current->GetDisplayName(context, NULL, &name);
    if (SUCCEEDED(hr) && name) {
      passed = !std::wcscmp(name, target);
      report("remote_moniker_equal", passed);
    }
    if (name)
      CoTaskMemFree(name);
    if (current)
      current->Release();
    if (persist)
      persist->Release();
    bound->Release();
  }
  if (callback->binding && !callback->stopped)
    callback->binding->Abort();
  if (source)
    source->Release();
  if (context)
    context->Release();
  callback->Release();
  return passed;
}
int main() {
  output =
      CreateFileA("C:\\ZUKUQA\\NAVTEST.LOG", GENERIC_WRITE, FILE_SHARE_READ,
                  NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (output == INVALID_HANDLE_VALUE)
    return 1;
  HRESULT hr = CoInitialize(NULL);
  report("CoInitialize", hr);
  if (FAILED(hr)) {
    CloseHandle(output);
    return 2;
  }
  /* The diagnostic registers the actual class factory only in this process.
     No DLL registry association, global HTTPS replacement, local HTML or fake
     engine is installed. */
  Factory *document = new Factory;
  DWORD registration = 0;
  hr =
      CoRegisterClassObject(IEWK_DOCUMENT_CLASS, document, CLSCTX_INPROC_SERVER,
                            REGCLS_MULTIPLEUSE, &registration);
  report("CoRegisterClassObject", hr);
  document->Release();
  IClassFactory *handoff = NULL;
  bool first = false, second = false, rejected = false;
  if (SUCCEEDED(hr))
    hr = iewk_handoff_attach(&handoff);
  report("attach", hr);
  if (SUCCEEDED(hr)) {
    HRESULT local = iewk_handoff_arm_get(handoff, L"file:///C:/test.html");
    HRESULT credentials =
        iewk_handoff_arm_get(handoff, L"https://u:p@example.test/");
    IClassFactory *duplicate = NULL;
    HRESULT duplicate_hr = iewk_handoff_attach(&duplicate);
    report("reject_local", local);
    report("reject_userinfo", credentials);
    report("reject_duplicate_adapter", duplicate_hr);
    rejected = local == E_INVALIDARG && credentials == E_INVALIDARG &&
               duplicate_hr == HRESULT_FROM_WIN32(ERROR_BUSY) && !duplicate;
    first = bind_document(handoff, L"https://www.zuzunza.com/");
    second = bind_document(handoff, L"https://example.test/navigation?q=one");
    report("detach", iewk_handoff_detach(handoff));
  }
  if (registration)
    CoRevokeClassObject(registration);
  report("all_moniker_checks", first && second && rejected);
  report("renderer_included", 0);
  report("tls_verified", 0);
  CoUninitialize();
  CloseHandle(output);
  return first && second && rejected ? 0 : 3;
}
