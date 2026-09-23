#ifndef IEWEBKIT_ENGINE_LOADER_H
#define IEWEBKIT_ENGINE_LOADER_H
#include "engine.h"
#include <windows.h>
struct IEWKLoadedEngine {
  HMODULE library;
  const IEWKEngineV1 *api;
};
int iewk_load_engine(HINSTANCE host, IEWKLoadedEngine *out);
void iewk_unload_engine(IEWKLoadedEngine *engine);
#endif
