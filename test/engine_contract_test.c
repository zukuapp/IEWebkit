#include "engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  IEWKEngineV1 e;
  memset(&e, 0, sizeof e);
  assert(iewk_engine_validate(NULL) == IEWK_MISSING_ENGINE);
  assert(iewk_engine_validate(&e) == IEWK_BAD_ABI);
  e.size = sizeof e;
  e.abi = IEWK_ABI_V1;
  assert(iewk_engine_validate(&e) == IEWK_INCOMPLETE_ENGINE);
  const char *good[] = {"https://example.org/", "http://www.zuzunza.com/",
                        "https://example.org/a?q=1#x",
                        "https://xn--bcher-kva.example/", NULL};
  const char *bad[] = {
      "file:///C:/renderer.html",         "javascript:alert(1)",
      "https://good.example@evil/",       "https://good.example\\evil/",
      "https://good.example%2fevil/",     "https:///empty",
      "https://example.org/\r\nCookie:x", NULL};
  int i;
  for (i = 0; good[i]; i++)
    assert(iewk_web_url(good[i], strlen(good[i])));
  for (i = 0; bad[i]; i++)
    assert(!iewk_web_url(bad[i], strlen(bad[i])));
  puts("engine contract: missing provider rejected; generic web URLs accepted; "
       "spoofing forms rejected");
  return 0;
}
