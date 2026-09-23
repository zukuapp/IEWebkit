#include "subset.h"
#include <cassert>
#include <iostream>
using namespace iewk;
static int measured(void *, const Font &font, const std::string &text) {
  int count = 0;
  for (size_t i = 0; i < text.size(); i++)
    if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80)
      count++;
  return count * font.pixels / 2;
}
static std::string visible(const SubsetDocument &doc) {
  std::string result;
  for (const Paint &p : doc.paint())
    if (p.kind == Paint::Text)
      result += p.text;
  return result;
}
static SubsetDocument document(const std::string &html, int width = 320) {
  SubsetDocument doc;
  assert(doc.append(html.data(), html.size(), true));
  assert(doc.layout(width, measured, NULL));
  return doc;
}
static int bad_measure(void *, const Font &, const std::string &) { return -1; }
int main() {
  SubsetDocument doc =
      document("<h1>Heading</h1><p>Hello <strong>world</strong></p>");
  assert(visible(doc) == "HeadingHello world");
  bool bold = false;
  for (const Paint &p : doc.paint())
    if (p.text == "world")
      bold = p.font.bold;
  assert(bold);
  doc = document("<style>p{color:#112233}.hot{color:#445566}#target{color:#"
                 "778899}</style><p id='target' class='hot' "
                 "style='color:#aabbcc'>Text</p><script>not visible</script>");
  assert(visible(doc) == "Text");
  assert(doc.paint().back().color == 0xaabbcc);
  doc = document(
      "<div style='padding:10px;background-color:#ffffff'><p>A &amp; B "
      "&#xD55C;&#44544;</p><p style='display:none'>hidden</p></div>");
  assert(visible(doc) == "A & B 한글");
  assert(doc.paint()[0].kind == Paint::Background);
  assert(doc.paint()[0].height > 20);
  doc = document("<p>one two three four</p>", 80);
  int first_y = -1, last_y = -1;
  for (const Paint &p : doc.paint())
    if (p.kind == Paint::Text) {
      if (first_y < 0)
        first_y = p.y;
      last_y = p.y;
      assert(p.x + p.width <= 80);
    }
  assert(last_y > first_y);
  doc =
      document("<a href='/safe'>safe</a> <a href='javascript:evil()'>bad</a>");
  assert(doc.link_at(doc.paint()[0].x, doc.paint()[0].y) == "/safe");
  for (const Paint &p : doc.paint())
    if (p.text == "bad")
      assert(p.href.empty());
  SubsetDocument streamed;
  std::string prefix = "<p>한", suffix = "글 &amp; done</p>";
  assert(streamed.append(prefix.data(), prefix.size() - 1));
  assert(streamed.layout(300, measured, NULL));
  assert(streamed.append(prefix.data() + prefix.size() - 1, 1));
  assert(streamed.append(suffix.data(), suffix.size(), true));
  assert(streamed.layout(300, measured, NULL));
  assert(visible(streamed) == "한글 & done");
  assert(!streamed.append("x", 1));
  doc = document(
      "<style>p {color:#010203</style><p>still visible</p><!--comment-->");
  assert(visible(doc) == "still visible");
  doc = document(
      "<script>hidden</scriptx><p>also hidden</p></script><p>safe</p>");
  assert(visible(doc) == "safe");
  SubsetDocument oversized;
  std::string huge(1024 * 1024 + 1, 'x');
  assert(!oversized.append(huge.data(), huge.size(), true));
  assert(!oversized.error().empty());
  const std::string incremental =
      "<style>p{color:#123456}</style><p>한글 &amp; &#x1F600;</p>";
  SubsetDocument every_byte;
  for (size_t i = 0; i < incremental.size(); ++i) {
    assert(every_byte.append(incremental.data() + i, 1,
                             i + 1 == incremental.size()));
    assert(every_byte.layout(300, measured, NULL));
  }
  assert(visible(every_byte) == visible(document(incremental)));
  const std::string before = visible(every_byte);
  assert(!every_byte.layout(0, measured, NULL));
  assert(!every_byte.layout(300, bad_measure, NULL));
  assert(visible(every_byte) == before);
  every_byte.reset();
  assert(every_byte.paint().empty() && !every_byte.complete());
  assert(!every_byte.append(NULL, 1));
  std::string deep, many, rules;
  for (int i = 0; i < 65; ++i)
    deep += "<div>";
  for (int i = 0; i < 4100; ++i)
    many += "<br>";
  for (int i = 0; i < 257; ++i)
    rules += "p{color:red}";
  for (const std::string &invalid :
       {deep, many, "<style>" + rules + "</style>"}) {
    SubsetDocument bounded;
    assert(bounded.append(invalid.data(), invalid.size(), true));
    assert(!bounded.layout(300, measured, NULL));
    assert(!bounded.error().empty());
  }
  doc = document("<p>한글한글한글한글</p>", 40);
  assert(visible(doc) == "한글한글한글한글");
  for (const Paint &p : doc.paint())
    assert(p.x + p.width <= 40);
  // Deterministic malformed-input corpus exercises parser boundaries under
  // ASan.
  unsigned state = 391;
  const std::string alphabet = "<>/='\";& abc#{}!\n한글";
  for (int trial = 0; trial < 500; ++trial) {
    std::string bytes;
    for (int i = 0; i < 200; ++i) {
      state = state * 1664525u + 1013904223u;
      bytes += alphabet[state % alphabet.size()];
    }
    SubsetDocument random;
    assert(random.append(bytes.data(), bytes.size(), true));
    random.layout(120, measured, NULL);
  }
  std::cout << "PASS subset: blocks, cascade, entities, UTF-8 streaming, "
               "wrapping, links, limits\n";
}
