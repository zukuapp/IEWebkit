#ifndef IEWK_SUBSET_WIN32_VIEW_H
#define IEWK_SUBSET_WIN32_VIEW_H
#include <stddef.h>
#include <windows.h>
namespace iewk {
/* Trusted native embedding API, not an engine ABI or script interface.
   parent must be the IE DocObject's real in-place parent on its owning STA.
   Link requests are inert unless the embedder supplies a callback; the embedder
   must resolve and validate each reference against its actual committed origin.
 */
typedef void (*SubsetLinkCallback)(void *context, const char *utf8,
                                   size_t length);
HWND create_subset_child(HWND parent, HINSTANCE instance,
                         SubsetLinkCallback link, void *context);
bool append_subset_html(HWND child, const char *utf8, size_t length,
                        bool final_chunk);
void reset_subset_html(HWND child);
} // namespace iewk
#endif
