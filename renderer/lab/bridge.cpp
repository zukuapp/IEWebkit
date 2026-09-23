/* A private fixed-file adapter for the explicit lab DocObject, NOT an exported
   IEWebKitGetEngineV1 provider. Normal engine_loader.cpp never loads this. */
#include "../../host/engine_loader.h"
#include "../win32_view.h"
#include "log.h"
#include "reference.h"
#include <cstring>
#include <new>
struct IEWKView {
  HWND child;
  IEWKHostV1 host;
};
typedef HWND(WINAPI *Create)(HWND, iewk::SubsetLinkCallback, void *);
typedef BOOL(WINAPI *Append)(HWND, const char *, size_t, BOOL);
typedef void(WINAPI *Reset)(HWND);
static Create create_child;
static Append append_html;
static Reset reset_html;
typedef BOOL(WINAPI *PrepareUnload)();
static PrepareUnload prepare_unload;
static const char fixture[] = "C:\\IEWKSUB\\DEMO.IWKSUBSET";
extern "C" int iewk_web_url(const char *value, size_t length) {
  return lab_reference(value, length);
}
static int IEWK_CALL create_view(const IEWKHostV1 *host, void *parent,
                                 IEWKView **out) {
  if (!host || !out || !create_child)
    return IEWK_BAD_ABI;
  *out = NULL;
  IEWKView *v = new (std::nothrow) IEWKView;
  if (!v)
    return IEWK_MISSING_ENGINE;
  v->host = *host;
  v->child = create_child(static_cast<HWND>(parent), NULL, NULL);
  lab_log("subset.child_created", v->child != NULL);
  if (!v->child) {
    delete v;
    return IEWK_MISSING_ENGINE;
  }
  *out = v;
  return IEWK_OK;
}
static void IEWK_CALL destroy(IEWKView *v) {
  if (v) {
    DestroyWindow(v->child);
    delete v;
    lab_log("subset.destroyed", 1);
  }
}
static int IEWK_CALL navigate(IEWKView *v, uint32_t, const char *url,
                              size_t length, const char *method,
                              size_t method_length, const void *,
                              size_t body_length) {
  if (!v || !iewk_web_url(url, length) || !method || method_length != 3 ||
      std::memcmp(method, "GET", 3) || body_length)
    return IEWK_BAD_ORIGIN;
  HANDLE file = CreateFileA(fixture, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    lab_log("subset.file_error", GetLastError());
    return IEWK_BAD_ORIGIN;
  }
  DWORD high = 0, size = GetFileSize(file, &high);
  if (high || size > 1024 * 1024 || size < 12) {
    CloseHandle(file);
    return IEWK_BAD_ORIGIN;
  }
  char signature[12];
  DWORD count;
  if (!ReadFile(file, signature, sizeof signature, &count, NULL) ||
      count != 12 || std::memcmp(signature, "IEWKSUBSET/1\n", 12)) {
    CloseHandle(file);
    return IEWK_BAD_ORIGIN;
  }
  reset_html(v->child);
  char chunk[1024];
  DWORD total = 0;
  BOOL ok = TRUE;
  while (ok) {
    if (!ReadFile(file, chunk, sizeof chunk, &count, NULL)) {
      ok = FALSE;
      break;
    }
    if (!count)
      break;
    total += count;
    ok = append_html(v->child, chunk, count, FALSE);
  }
  CloseHandle(file);
  if (ok)
    ok = append_html(v->child, NULL, 0, TRUE);
  lab_log("subset.bytes", total);
  lab_log("subset.layout_ok", ok);
  if (ok && v->host.title) {
    const char title[] = "IEWebkit local HTML subset lab (no WebKit or TLS)";
    v->host.title(v->host.context, title, sizeof title - 1);
  }
  return ok ? IEWK_OK : IEWK_BAD_ABI;
}
static void IEWK_CALL resize(IEWKView *v, int x, int y, int w, int h) {
  if (v)
    MoveWindow(v->child, x, y, w, h, TRUE);
}
static void IEWK_CALL focus(IEWKView *v, int active) {
  if (v && active)
    SetFocus(v->child);
}
static int IEWK_CALL command(IEWKView *, uint32_t) { return IEWK_BAD_ABI; }
static const IEWKEngineV1 api = {sizeof(IEWKEngineV1),
                                 IEWK_ABI_V1,
                                 IEWK_HTML | IEWK_CSS,
                                 "Explicit local subset lab; incomplete engine",
                                 "",
                                 create_view,
                                 destroy,
                                 navigate,
                                 resize,
                                 focus,
                                 command};
int iewk_load_engine(HINSTANCE module, IEWKLoadedEngine *out) {
  if (!module || !out)
    return IEWK_BAD_ABI;
  out->library = NULL;
  out->api = NULL;
  char path[MAX_PATH];
  DWORD n = GetModuleFileNameA(module, path, sizeof path);
  if (!n || n >= sizeof path)
    return IEWK_MISSING_ENGINE;
  char *slash = std::strrchr(path, '\\');
  if (!slash ||
      static_cast<size_t>(slash - path) + sizeof "SUBSET.DLL" >= sizeof path)
    return IEWK_MISSING_ENGINE;
  std::strcpy(slash + 1, "SUBSET.DLL");
  HMODULE library = LoadLibraryA(path);
  if (!library) {
    lab_log("subset.dll_error", GetLastError());
    return IEWK_MISSING_ENGINE;
  }
  FARPROC a = GetProcAddress(library, "IEWKSubsetCreate"),
          b = GetProcAddress(library, "IEWKSubsetAppend"),
          c = GetProcAddress(library, "IEWKSubsetReset");
  std::memcpy(&create_child, &a, sizeof a);
  std::memcpy(&append_html, &b, sizeof b);
  std::memcpy(&reset_html, &c, sizeof c);
  FARPROC d = GetProcAddress(library, "IEWKSubsetPrepareUnload");
  std::memcpy(&prepare_unload, &d, sizeof d);
  if (!a || !b || !c || !d) {
    FreeLibrary(library);
    return IEWK_BAD_ABI;
  }
  out->library = library;
  out->api = &api;
  return IEWK_OK;
}
void iewk_unload_engine(IEWKLoadedEngine *engine) {
  if (engine && engine->library) {
    // Another live DocObject retains its own module reference if class removal
    // fails because that document still owns a child window.
    if (prepare_unload)
      prepare_unload();
    FreeLibrary(engine->library);
    engine->library = NULL;
    engine->api = NULL;
  }
}
