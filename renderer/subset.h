#ifndef IEWK_SUBSET_RENDERER_H
#define IEWK_SUBSET_RENDERER_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace iewk {
struct Font {
  int pixels;
  bool bold, italic;
};
struct Style {
  Font font;
  uint32_t color, background;
  bool has_background, hidden, block, underline;
  int margin, padding;
};
struct Paint {
  enum Kind { Background, Text } kind;
  int x, y, width, height;
  Font font;
  uint32_t color;
  std::string text, href;
  bool underline;
};
typedef int (*Measure)(void *, const Font &, const std::string &);
class SubsetDocument {
public:
  SubsetDocument();
  bool append(const char *bytes, size_t length, bool final_chunk = false);
  void reset();
  bool layout(int width, Measure measure, void *context);
  const std::vector<Paint> &paint() const { return paints_; }
  int height() const { return height_; }
  std::string link_at(int x, int y) const;
  bool complete() const { return complete_; }
  const std::string &error() const { return error_; }

private:
  std::string html_, error_;
  bool complete_;
  int height_;
  std::vector<Paint> paints_;
};
} // namespace iewk
#endif
