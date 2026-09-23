#ifndef IEWK_NAVIGATION_HANDOFF_H
#define IEWK_NAVIGATION_HANDOFF_H
#include <urlmon.h>
#include <windows.h>

/* Development prototype: one owning STA and one pending, exact URL ticket.
   This selects a DocObject; it supplies no HTML, origin assertion or TLS
   result. A browser adapter may arm only its own top-level GET navigation after
   engine availability checks. The diagnostic harness exercises selection
   separately. */
HRESULT iewk_handoff_attach(IClassFactory **factory);
HRESULT iewk_handoff_arm_get(IClassFactory *factory, LPCWSTR url);
HRESULT iewk_handoff_cancel(IClassFactory *factory);
HRESULT iewk_handoff_detach(IClassFactory *factory);
LONG iewk_handoff_live_objects();
extern const CLSID IEWK_DOCUMENT_CLASS;
#endif
