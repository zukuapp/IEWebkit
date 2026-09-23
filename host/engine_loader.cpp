#include "engine_loader.h"
#include <cstring>
int iewk_load_engine(HINSTANCE host, IEWKLoadedEngine *out) {
  char path[MAX_PATH];
  DWORD n;
  char *slash;
  IEWKGetEngineV1 getter;
  int result;
  if (!host || !out)
    return IEWK_BAD_ABI;
  out->library = NULL;
  out->api = NULL;
  n = GetModuleFileNameA(host, path, sizeof path);
  if (!n || n >= sizeof path)
    return IEWK_MISSING_ENGINE;
  slash = std::strrchr(path, '\\');
  if (!slash ||
      static_cast<size_t>(slash - path) + sizeof "iewebkit-engine.dll" >=
          sizeof path)
    return IEWK_MISSING_ENGINE;
  std::strcpy(slash + 1, "iewebkit-engine.dll");
  out->library = LoadLibraryA(path);
  if (!out->library)
    return IEWK_MISSING_ENGINE;
  FARPROC address = GetProcAddress(out->library, "IEWebKitGetEngineV1");
  static_assert(sizeof getter == sizeof address, "Win32 function-pointer ABI");
  std::memcpy(&getter, &address, sizeof getter);
  result = getter ? getter(IEWK_ABI_V1, &out->api) : IEWK_BAD_ABI;
  if (result == IEWK_OK)
    result = iewk_engine_validate(out->api);
  if (result != IEWK_OK) {
    FreeLibrary(out->library);
    out->library = NULL;
    out->api = NULL;
  }
  return result;
}
void iewk_unload_engine(IEWKLoadedEngine *engine) {
  if (engine && engine->library) {
    FreeLibrary(engine->library);
    engine->library = NULL;
    engine->api = NULL;
  }
}
