#ifndef IEWK_SUBSET_LAB_REFERENCE_H
#define IEWK_SUBSET_LAB_REFERENCE_H
#include <cstddef>
#include <cstring>
static inline bool lab_reference(const char *value, size_t length) {
  const char *allowed[] = {
      "file:///C:/IEWKSUB/DEMO.IWKSUBSET", "file://C:/IEWKSUB/DEMO.IWKSUBSET",
      "file:C:/IEWKSUB/DEMO.IWKSUBSET", "C:\\IEWKSUB\\DEMO.IWKSUBSET"};
  if (!value || length > 100)
    return false;
  for (const char *path : allowed) {
    if (length != std::strlen(path))
      continue;
    bool equal = true;
    for (size_t i = 0; i < length; ++i) {
      unsigned char a = value[i], b = path[i];
      if (a >= 'a' && a <= 'z')
        a -= 32;
      if (b >= 'a' && b <= 'z')
        b -= 32;
      if (a != b) {
        equal = false;
        break;
      }
    }
    if (equal)
      return true;
  }
  return false;
}
#endif
