#include "win32_view.h"
#include "subset.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <new>
#include <vector>
namespace iewk {
namespace {
const char *window_class = "IEWebkitSubsetChildV1";
COLORREF rgb(uint32_t color) {
  return RGB((color >> 16) & 255, (color >> 8) & 255, color & 255);
}
std::string ansi(const std::string &text) {
  if (text.empty())
    return std::string();
  int n = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                              static_cast<int>(text.size()), NULL, 0);
  if (n <= 0)
    return std::string();
  std::vector<WCHAR> wide(static_cast<size_t>(n));
  if (!MultiByteToWideChar(CP_UTF8, 0, text.data(),
                           static_cast<int>(text.size()), wide.data(), n))
    return std::string();
  int bytes =
      WideCharToMultiByte(CP_ACP, 0, wide.data(), n, NULL, 0, NULL, NULL);
  if (bytes <= 0)
    return std::string();
  std::string result(static_cast<size_t>(bytes), '\0');
  WideCharToMultiByte(CP_ACP, 0, wide.data(), n, &result[0], bytes, NULL, NULL);
  return result;
}
struct View {
  HWND window;
  SubsetDocument document;
  std::map<int, HFONT> fonts;
  int scroll;
  SubsetLinkCallback link;
  void *context;
  View() : window(NULL), scroll(0), link(NULL), context(NULL) {}
  ~View() {
    for (auto &entry : fonts)
      DeleteObject(entry.second);
  }
  HFONT font(const Font &spec) {
    int key = spec.pixels * 4 + (spec.bold ? 2 : 0) + (spec.italic ? 1 : 0);
    auto found = fonts.find(key);
    if (found != fonts.end())
      return found->second;
    HFONT created = CreateFontA(
        -spec.pixels, 0, 0, 0, spec.bold ? FW_BOLD : FW_NORMAL, spec.italic,
        FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH, GetACP() == 949 ? "Gulim" : "Arial");
    if (created) {
      try {
        fonts[key] = created;
      } catch (...) {
        DeleteObject(created);
        throw;
      }
    }
    return created ? created
                   : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  }
  struct MeasureContext {
    View *view;
    HDC dc;
  };
  static int measure(void *opaque, const Font &spec, const std::string &text) {
    MeasureContext *m = static_cast<MeasureContext *>(opaque);
    std::string bytes = ansi(text);
    if (bytes.empty() && !text.empty())
      return -1;
    HGDIOBJ old = SelectObject(m->dc, m->view->font(spec));
    SIZE size = {0, 0};
    BOOL ok = GetTextExtentPoint32A(m->dc, bytes.data(),
                                    static_cast<int>(bytes.size()), &size);
    SelectObject(m->dc, old);
    return ok ? size.cx : -1;
  }
  void scroll_to(int next) {
    RECT rect;
    GetClientRect(window, &rect);
    scroll = std::max(
        0, std::min(next, std::max(0, document.height() -
                                          static_cast<int>(rect.bottom))));
    SCROLLINFO info;
    std::memset(&info, 0, sizeof info);
    info.cbSize = sizeof info;
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMax = std::max(0, document.height() - 1);
    info.nPage = rect.bottom;
    info.nPos = scroll;
    SetScrollInfo(window, SB_VERT, &info, TRUE);
    InvalidateRect(window, NULL, TRUE);
  }
  bool layout() {
    RECT rect;
    GetClientRect(window, &rect);
    if (rect.right < 1)
      return true;
    HDC dc = GetDC(window);
    if (!dc)
      return false;
    MeasureContext measure_context = {this, dc};
    bool ok = document.layout(rect.right, measure, &measure_context);
    ReleaseDC(window, dc);
    scroll_to(scroll);
    return ok;
  }
  void paint(HDC dc) {
    RECT client;
    GetClientRect(window, &client);
    FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    SetBkMode(dc, TRANSPARENT);
    for (const Paint &p : document.paint()) {
      int top = p.y - scroll;
      if (top + p.height < 0 || top >= client.bottom)
        continue;
      if (p.kind == Paint::Background) {
        RECT box = {p.x, top, p.x + p.width, top + p.height};
        HBRUSH brush = CreateSolidBrush(rgb(p.color));
        if (brush) {
          FillRect(dc, &box, brush);
          DeleteObject(brush);
        }
        continue;
      }
      HGDIOBJ old = SelectObject(dc, font(p.font));
      SetTextColor(dc, rgb(p.color));
      std::string bytes = ansi(p.text);
      TextOutA(dc, p.x, top, bytes.data(), static_cast<int>(bytes.size()));
      SelectObject(dc, old);
      if (p.underline) {
        HPEN pen = CreatePen(PS_SOLID, 1, rgb(p.color));
        if (pen) {
          HGDIOBJ previous = SelectObject(dc, pen);
          MoveToEx(dc, p.x, top + p.font.pixels + 1, NULL);
          LineTo(dc, p.x + p.width, top + p.font.pixels + 1);
          SelectObject(dc, previous);
          DeleteObject(pen);
        }
      }
    }
  }
};
struct Creation {
  View *view;
  bool accepted;
};
View *get(HWND window) {
  char name[64];
  if (!GetClassNameA(window, name, sizeof name) ||
      std::strcmp(name, window_class))
    return NULL;
  return reinterpret_cast<View *>(GetWindowLongPtrA(window, GWLP_USERDATA));
}
LRESULT procedure_impl(HWND window, UINT message, WPARAM wparam,
                       LPARAM lparam) {
  View *view =
      reinterpret_cast<View *>(GetWindowLongPtrA(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    CREATESTRUCTA *created = reinterpret_cast<CREATESTRUCTA *>(lparam);
    Creation *creation = static_cast<Creation *>(created->lpCreateParams);
    view = creation->view;
    creation->accepted = true;
    view->window = window;
    SetWindowLongPtrA(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
    return TRUE;
  }
  if (!view)
    return DefWindowProcA(window, message, wparam, lparam);
  switch (message) {
  case WM_SIZE:
    view->layout();
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(window, &ps);
    try {
      view->paint(dc);
    } catch (const std::exception
                 &) { /* Keep WM_PAINT balanced on allocation failure. */
    }
    EndPaint(window, &ps);
    return 0;
  }
  case WM_VSCROLL: {
    SCROLLINFO info;
    std::memset(&info, 0, sizeof info);
    info.cbSize = sizeof info;
    info.fMask = SIF_TRACKPOS;
    GetScrollInfo(window, SB_VERT, &info);
    RECT rect;
    GetClientRect(window, &rect);
    int next = view->scroll;
    switch (LOWORD(wparam)) {
    case SB_LINEUP:
      next -= 24;
      break;
    case SB_LINEDOWN:
      next += 24;
      break;
    case SB_PAGEUP:
      next -= rect.bottom;
      break;
    case SB_PAGEDOWN:
      next += rect.bottom;
      break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
      next = info.nTrackPos;
      break;
    case SB_TOP:
      next = 0;
      break;
    case SB_BOTTOM:
      next = view->document.height();
      break;
    }
    view->scroll_to(next);
    return 0;
  }
  case WM_MOUSEWHEEL:
    view->scroll_to(view->scroll -
                    static_cast<short>(HIWORD(wparam)) * 72 / WHEEL_DELTA);
    return 0;
  case WM_KEYDOWN: {
    RECT rect;
    GetClientRect(window, &rect);
    int next = view->scroll;
    switch (wparam) {
    case VK_UP:
      next -= 24;
      break;
    case VK_DOWN:
      next += 24;
      break;
    case VK_PRIOR:
      next -= rect.bottom;
      break;
    case VK_NEXT:
      next += rect.bottom;
      break;
    case VK_HOME:
      next = 0;
      break;
    case VK_END:
      next = view->document.height();
      break;
    default:
      return DefWindowProcA(window, message, wparam, lparam);
    }
    view->scroll_to(next);
    return 0;
  }
  case WM_LBUTTONUP: {
    SetFocus(window);
    std::string href = view->document.link_at(
        static_cast<short>(LOWORD(lparam)),
        static_cast<short>(HIWORD(lparam)) + view->scroll);
    if (!href.empty() && view->link)
      view->link(view->context, href.data(), href.size());
    return 0;
  }
  case WM_NCDESTROY:
    SetWindowLongPtrA(window, GWLP_USERDATA, 0);
    delete view;
    return DefWindowProcA(window, message, wparam, lparam);
  default:
    return DefWindowProcA(window, message, wparam, lparam);
  }
}
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wparam,
                           LPARAM lparam) {
  try {
    return procedure_impl(window, message, wparam, lparam);
  } catch (...) {
    return message == WM_NCCREATE ? FALSE : 0;
  }
}
} // namespace
HWND create_subset_child(HWND parent, HINSTANCE instance,
                         SubsetLinkCallback link, void *context) {
  if (!IsWindow(parent) ||
      GetWindowThreadProcessId(parent, NULL) != GetCurrentThreadId())
    return NULL;
  WNDCLASSA type;
  std::memset(&type, 0, sizeof type);
  type.hInstance = instance;
  type.lpfnWndProc = procedure;
  type.lpszClassName = window_class;
  type.hCursor = LoadCursorA(NULL, IDC_ARROW);
  if (!RegisterClassA(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return NULL;
  View *view = new (std::nothrow) View;
  if (!view)
    return NULL;
  view->link = link;
  view->context = context;
  RECT rect;
  GetClientRect(parent, &rect);
  Creation creation = {view, false};
  HWND window = CreateWindowA(
      window_class, "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP, 0, 0,
      rect.right, rect.bottom, parent, NULL, instance, &creation);
  /* WM_NCDESTROY owns View after successful WM_NCCREATE. If allocation fails
     before that message, CreateWindow cannot have installed a window handle. */
  if (!window && !creation.accepted)
    delete view;
  return window;
}
bool append_subset_html(HWND child, const char *utf8, size_t length,
                        bool final_chunk) {
  View *view = get(child);
  if (!view || GetWindowThreadProcessId(child, NULL) != GetCurrentThreadId())
    return false;
  try {
    return view->document.append(utf8, length, final_chunk) && view->layout();
  } catch (const std::exception &) {
    return false;
  }
}
void reset_subset_html(HWND child) {
  View *view = get(child);
  if (view && GetWindowThreadProcessId(child, NULL) == GetCurrentThreadId()) {
    view->document.reset();
    view->scroll_to(0);
  }
}
} // namespace iewk
