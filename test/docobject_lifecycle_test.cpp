/* Exercises the actual COM implementation. No mock renderer or rendered pass.
 */
#include "../host/docobject.cpp"
#include <cassert>
struct TestSite : IOleClientSite, IOleInPlaceSite {
  LONG refs, deactivated;
  TestSite() : refs(1), deactivated(0) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) {
    if (!out)
      return E_POINTER;
    *out = NULL;
    if (IsEqualIID(id, IID_IUnknown) || IsEqualIID(id, IID_IOleClientSite))
      *out = static_cast<IOleClientSite *>(this);
    else if (IsEqualIID(id, IID_IOleInPlaceSite) ||
             IsEqualIID(id, IID_IOleWindow))
      *out = static_cast<IOleInPlaceSite *>(this);
    if (!*out)
      return E_NOINTERFACE;
    AddRef();
    return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() { return ++refs; }
  ULONG STDMETHODCALLTYPE Release() { return --refs; }
  HRESULT STDMETHODCALLTYPE SaveObject() { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE GetMoniker(DWORD, DWORD, IMoniker **out) {
    if (out)
      *out = NULL;
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer **out) {
    if (out)
      *out = NULL;
    return E_NOINTERFACE;
  }
  HRESULT STDMETHODCALLTYPE ShowObject() { return S_OK; }
  HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL) { return S_OK; }
  HRESULT STDMETHODCALLTYPE RequestNewObjectLayout() { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE GetWindow(HWND *out) {
    if (!out)
      return E_POINTER;
    *out = NULL;
    return E_FAIL;
  }
  HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL) { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE CanInPlaceActivate() { return S_OK; }
  HRESULT STDMETHODCALLTYPE OnInPlaceActivate() { return S_OK; }
  HRESULT STDMETHODCALLTYPE OnUIActivate() { return S_OK; }
  HRESULT STDMETHODCALLTYPE GetWindowContext(IOleInPlaceFrame **frame,
                                             IOleInPlaceUIWindow **doc, LPRECT,
                                             LPRECT, LPOLEINPLACEFRAMEINFO) {
    if (frame)
      *frame = NULL;
    if (doc)
      *doc = NULL;
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE Scroll(SIZE) { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL) { return S_OK; }
  HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate() {
    ++deactivated;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE DiscardUndoState() { return S_OK; }
  HRESULT STDMETHODCALLTYPE DeactivateAndUndo() { return S_OK; }
  HRESULT STDMETHODCALLTYPE OnPosRectChange(LPCRECT) { return S_OK; }
};
int main() {
  CoInitialize(NULL);
  TestSite first, second;
  Document *doc = new Document;
  assert(doc->SetClientSite(&first) == S_OK);
  assert(doc->SetInPlaceSite(&first) == S_OK);
  assert(first.refs == 3);
  assert(doc->SetClientSite(&first) == S_OK && first.refs == 3);
  assert(doc->SetClientSite(&second) == S_OK);
  assert(first.refs == 1 && second.refs == 2);
  IOleInPlaceSite *place = reinterpret_cast<IOleInPlaceSite *>(1);
  assert(doc->GetInPlaceSite(&place) == S_OK && place == NULL);
  assert(doc->SetInPlaceSite(&second) == S_OK);
  assert(doc->SetInPlaceSite(&first) == S_OK);
  assert(second.refs == 2 && first.refs == 2);
  assert(doc->SetObjectRects(NULL, NULL) == E_POINTER);
  assert(doc->SetClientSite(NULL) == S_OK);
  assert(first.refs == 1 && second.refs == 1);
  assert(doc->Release() == 0);
  assert(objects == 0);
  FILE *log = std::fopen("C:\\ZUKUQA\\HOSTTEST.LOG", "wb");
  if (log) {
    std::fputs("PASS DocObject site replacement, reference lifetime and null "
               "rect checks\r\n",
               log);
    std::fclose(log);
  }
  CoUninitialize();
  return 0;
}
