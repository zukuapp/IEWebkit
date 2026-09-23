#include "subset.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <stdexcept>
namespace iewk {
namespace {
const size_t input_limit = 1024 * 1024, node_limit = 4096, paint_limit = 65536;
bool space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}
std::string trim(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && space(s[a]))
    a++;
  while (b > a && space(s[b - 1]))
    b--;
  return s.substr(a, b - a);
}
std::string lower(std::string s) {
  for (char &c : s)
    if (c >= 'A' && c <= 'Z')
      c += 32;
  return s;
}
bool ident(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_';
}
void encode(std::string &out, unsigned cp) {
  if (!cp || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
    cp = 0xfffd;
  if (cp < 0x80)
    out += static_cast<char>(cp);
  else if (cp < 0x800) {
    out += static_cast<char>(0xc0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 63));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xe0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 63));
    out += static_cast<char>(0x80 | (cp & 63));
  } else {
    out += static_cast<char>(0xf0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 63));
    out += static_cast<char>(0x80 | ((cp >> 6) & 63));
    out += static_cast<char>(0x80 | (cp & 63));
  }
}
std::string decoded(const std::string &in, bool final) {
  std::string out;
  for (size_t i = 0; i < in.size();) {
    unsigned char c = in[i];
    if (c == '&') {
      size_t end = in.find(';', i + 1);
      if (end == std::string::npos && !final)
        break;
      if (end != std::string::npos && end - i <= 16) {
        std::string n = in.substr(i + 1, end - i - 1);
        unsigned cp = 0;
        if (n == "amp")
          cp = '&';
        else if (n == "lt")
          cp = '<';
        else if (n == "gt")
          cp = '>';
        else if (n == "quot")
          cp = '"';
        else if (n == "apos")
          cp = '\'';
        else if (n == "nbsp")
          cp = 160;
        else if (n.size() > 1 && n[0] == '#') {
          char *tail = NULL;
          const char *d = n.c_str() + 1;
          int base = 10;
          if (*d == 'x' || *d == 'X') {
            base = 16;
            d++;
          }
          unsigned long v = std::strtoul(d, &tail, base);
          if (*d && tail && !*tail)
            cp = v <= 0x10ffff ? static_cast<unsigned>(v) : 0xfffd;
        }
        if (cp) {
          encode(out, cp);
          i = end + 1;
          continue;
        }
      }
    }
    if (c < 0x80) {
      if (c)
        out += static_cast<char>(c);
      else
        encode(out, 0xfffd);
      i++;
      continue;
    }
    size_t n = c >= 0xc2 && c <= 0xdf   ? 2
               : c >= 0xe0 && c <= 0xef ? 3
               : c >= 0xf0 && c <= 0xf4 ? 4
                                        : 0;
    if (n && i + n > in.size() && !final)
      break;
    bool valid = n && i + n <= in.size();
    unsigned cp = n ? c & ((1u << (7 - n)) - 1) : 0;
    for (size_t j = 1; valid && j < n; j++) {
      unsigned char next = in[i + j];
      valid = (next & 0xc0) == 0x80;
      cp = (cp << 6) | (next & 63);
    }
    valid = valid &&
            cp >= (n == 2   ? 0x80u
                   : n == 3 ? 0x800u
                            : 0x10000u) &&
            cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
    if (valid) {
      out.append(in, i, n);
      i += n;
    } else {
      encode(out, 0xfffd);
      i++;
    }
  }
  return out;
}
typedef std::map<std::string, std::string> Props;
Props declarations(const std::string &s) {
  Props out;
  size_t p = 0;
  while (p < s.size()) {
    size_t end = s.find(';', p);
    if (end == std::string::npos)
      end = s.size();
    size_t colon = s.find(':', p);
    if (colon < end) {
      std::string k = lower(trim(s.substr(p, colon - p))),
                  v = lower(trim(s.substr(colon + 1, end - colon - 1)));
      if (k.size() < 40 && v.size() < 100)
        out[k] = v;
    }
    p = end + 1;
  }
  return out;
}
struct Node {
  std::string tag, text, id, classes, href, css;
  std::vector<size_t> children;
};
struct Rule {
  std::string selector;
  Props props;
  int specificity;
};
struct Tree {
  std::vector<Node> nodes;
  std::vector<Rule> rules;
};
bool valid_selector(const std::string &s) {
  if (s == "*")
    return true;
  size_t p = !s.empty() && (s[0] == '.' || s[0] == '#') ? 1 : 0;
  if (p == s.size())
    return false;
  for (; p < s.size(); p++)
    if (!ident(s[p]))
      return false;
  return true;
}
void stylesheet(Tree &t, const std::string &s) {
  size_t p = 0;
  while (p < s.size()) {
    size_t a = s.find('{', p), b = s.find('}', a == std::string::npos ? p : a);
    if (a == std::string::npos || b == std::string::npos)
      break;
    Props props = declarations(s.substr(a + 1, b - a - 1));
    std::string selectors = s.substr(p, a - p);
    size_t start = 0;
    while (start < selectors.size()) {
      size_t end = selectors.find(',', start);
      if (end == std::string::npos)
        end = selectors.size();
      std::string name = trim(selectors.substr(start, end - start));
      if (valid_selector(name)) {
        if (t.rules.size() >= 256)
          throw std::length_error("CSS rule limit exceeded");
        Rule r = {name, props,
                  name[0] == '#'   ? 100
                  : name[0] == '.' ? 10
                  : name == "*"    ? 0
                                   : 1};
        t.rules.push_back(r);
      }
      start = end + 1;
    }
    p = b + 1;
  }
}
Props attributes(const std::string &s, size_t p) {
  Props out;
  while (p < s.size()) {
    while (p < s.size() && (space(s[p]) || s[p] == '/'))
      p++;
    size_t start = p;
    while (p < s.size() && (ident(s[p]) || s[p] == ':'))
      p++;
    if (start == p) {
      p++;
      continue;
    }
    std::string name = lower(s.substr(start, p - start));
    while (p < s.size() && space(s[p]))
      p++;
    std::string value;
    if (p < s.size() && s[p] == '=') {
      p++;
      while (p < s.size() && space(s[p]))
        p++;
      char q = p < s.size() && (s[p] == '\'' || s[p] == '"') ? s[p++] : 0;
      start = p;
      while (p < s.size() && (q ? s[p] != q : !space(s[p])))
        p++;
      value = decoded(s.substr(start, p - start), true);
      if (q && p < s.size())
        p++;
    }
    out[name] = value;
  }
  return out;
}
size_t add(Tree &t, size_t parent, const Node &n) {
  if (t.nodes.size() >= node_limit)
    throw std::length_error("HTML node limit exceeded");
  size_t index = t.nodes.size();
  t.nodes.push_back(n);
  t.nodes[parent].children.push_back(index);
  return index;
}
Tree parse(const std::string &s, bool final) {
  Tree t;
  Node root;
  root.tag = "root";
  t.nodes.push_back(root);
  std::vector<size_t> stack(1, 0);
  std::string folded = lower(s);
  size_t p = 0;
  while (p < s.size()) {
    if (s[p] != '<') {
      size_t end = s.find('<', p);
      if (end == std::string::npos)
        end = s.size();
      Node n;
      n.tag = "#text";
      n.text = decoded(s.substr(p, end - p), end < s.size() || final);
      if (!n.text.empty())
        add(t, stack.back(), n);
      p = end;
      continue;
    }
    if (s.compare(p, 4, "<!--") == 0) {
      size_t end = s.find("-->", p + 4);
      if (end == std::string::npos)
        break;
      p = end + 3;
      continue;
    }
    size_t end = p + 1;
    char q = 0;
    for (; end < s.size(); end++) {
      char c = s[end];
      if (q) {
        if (c == q)
          q = 0;
      } else if (c == '\'' || c == '"')
        q = c;
      else if (c == '>')
        break;
    }
    if (end == s.size())
      break;
    std::string content = trim(s.substr(p + 1, end - p - 1));
    p = end + 1;
    if (content.empty() || content[0] == '!')
      continue;
    bool closing = content[0] == '/';
    size_t start = closing ? 1 : 0, pos = start;
    while (pos < content.size() && ident(content[pos]))
      pos++;
    std::string tag = lower(content.substr(start, pos - start));
    if (tag.empty())
      continue;
    if (closing) {
      for (size_t i = stack.size(); i > 1; i--)
        if (t.nodes[stack[i - 1]].tag == tag) {
          stack.resize(i - 1);
          break;
        }
      continue;
    }
    if (tag == "script" || tag == "style") {
      size_t close = folded.find("</" + tag, p);
      while (close != std::string::npos) {
        size_t after = close + tag.size() + 2;
        if (after < folded.size() &&
            (space(folded[after]) || folded[after] == '>' ||
             folded[after] == '/'))
          break;
        close = folded.find("</" + tag, after);
      }
      if (close == std::string::npos)
        break;
      if (tag == "style")
        stylesheet(t, s.substr(p, close - p));
      p = close;
      continue;
    }
    Props a = attributes(content, pos);
    Node n;
    n.tag = tag;
    n.id = a["id"];
    n.classes = a["class"];
    n.href = a["href"];
    n.css = a["style"];
    size_t index = add(t, stack.back(), n);
    bool leaf = tag == "br" || tag == "hr" || tag == "img" || tag == "input" ||
                tag == "meta" || tag == "link" || content.back() == '/';
    if (!leaf) {
      if (stack.size() >= 64)
        throw std::length_error("HTML nesting limit exceeded");
      stack.push_back(index);
    }
  }
  std::stable_sort(t.rules.begin(), t.rules.end(),
                   [](const Rule &a, const Rule &b) {
                     return a.specificity < b.specificity;
                   });
  return t;
}
bool matches(const Rule &r, const Node &n) {
  if (r.selector == "*")
    return true;
  if (r.selector[0] == '#')
    return n.id == r.selector.substr(1);
  if (r.selector[0] == '.') {
    std::string all = " ";
    for (char c : n.classes)
      all += space(c) ? ' ' : c;
    all += ' ';
    return all.find(" " + r.selector.substr(1) + " ") != std::string::npos;
  }
  return lower(r.selector) == n.tag;
}
bool pixels(const std::string &s, int &out, int maximum) {
  if (s.empty())
    return false;
  char *tail = NULL;
  long n = std::strtol(s.c_str(), &tail, 10);
  if (tail == s.c_str() || (*tail && std::string(tail) != "px") || n < 0 ||
      n > maximum)
    return false;
  out = static_cast<int>(n);
  return true;
}
bool color(const std::string &s, uint32_t &out) {
  if (s == "black") {
    out = 0;
    return true;
  }
  if (s == "white") {
    out = 0xffffff;
    return true;
  }
  if (s == "red") {
    out = 0xff0000;
    return true;
  }
  if (s == "blue") {
    out = 0xff;
    return true;
  }
  if (s == "green") {
    out = 0x8000;
    return true;
  }
  if ((s.size() != 4 && s.size() != 7) || s[0] != '#')
    return false;
  uint32_t v = 0;
  for (size_t i = 1; i < s.size(); i++) {
    char c = s[i];
    unsigned d = c >= '0' && c <= '9'   ? c - '0'
                 : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                        : 99;
    if (d > 15)
      return false;
    v = s.size() == 4 ? (v << 8) | (d * 17) : (v << 4) | d;
  }
  out = v;
  return true;
}
void apply(Style &s, const Props &p) {
  for (const auto &e : p) {
    const std::string &k = e.first, &v = e.second;
    if (k == "color")
      color(v, s.color);
    else if (k == "background-color") {
      if (v == "transparent")
        s.has_background = false;
      else if (color(v, s.background))
        s.has_background = true;
    } else if (k == "font-size") {
      int size = 0;
      if (pixels(v, size, 96) && size >= 6)
        s.font.pixels = size;
    } else if (k == "font-weight") {
      if (v == "bold" || v == "700")
        s.font.bold = true;
      else if (v == "normal" || v == "400")
        s.font.bold = false;
    } else if (k == "font-style") {
      if (v == "italic")
        s.font.italic = true;
      else if (v == "normal")
        s.font.italic = false;
    } else if (k == "text-decoration") {
      if (v == "underline")
        s.underline = true;
      else if (v == "none")
        s.underline = false;
    } else if (k == "padding")
      pixels(v, s.padding, 256);
    else if (k == "margin")
      pixels(v, s.margin, 256);
    else if (k == "display") {
      if (v == "none")
        s.hidden = true;
      else if (v == "block") {
        s.hidden = false;
        s.block = true;
      } else if (v == "inline") {
        s.hidden = false;
        s.block = false;
      }
    }
  }
}
Style computed(const Node &n, const Style &parent,
               const std::vector<Rule> &rules) {
  Style s = parent;
  s.has_background = false;
  s.hidden = false;
  s.margin = 0;
  s.padding = 0;
  s.block = false;
  const std::string &t = n.tag;
  s.block = t == "root" || t == "html" || t == "body" || t == "div" ||
            t == "p" || t == "section" || t == "article" || t == "header" ||
            t == "footer" || t == "ul" || t == "ol" || t == "li" || t == "h1" ||
            t == "h2" || t == "h3";
  if (t == "p")
    s.margin = 8;
  if (t == "h1" || t == "h2" || t == "h3") {
    s.font.pixels = t == "h1" ? 28 : t == "h2" ? 24 : 20;
    s.font.bold = true;
    s.margin = 8;
  }
  if (t == "strong" || t == "b")
    s.font.bold = true;
  if (t == "em" || t == "i")
    s.font.italic = true;
  if (t == "a") {
    s.color = 0xee;
    s.underline = true;
  }
  if (t == "head" || t == "title" || t == "template")
    s.hidden = true;
  for (const Rule &r : rules)
    if (matches(r, n))
      apply(s, r.props);
  apply(s, declarations(n.css));
  return s;
}
std::string safe_href(std::string href) {
  href = trim(href);
  for (unsigned char c : href)
    if (c < 32 || c == 127)
      return std::string();
  size_t colon = href.find(':'), boundary = href.find_first_of("/?#");
  if (colon != std::string::npos &&
      (boundary == std::string::npos || colon < boundary)) {
    std::string scheme = lower(href.substr(0, colon));
    if (scheme != "http" && scheme != "https")
      return std::string();
  }
  return href;
}
struct Flow {
  int left, right, x, y, line_height;
  bool pending_space, started;
};
class Layout {
  const Tree &tree;
  Measure measure;
  void *context;
  std::vector<Paint> &out;
  void emit(Paint p) {
    if (out.size() >= paint_limit)
      throw std::length_error("paint command limit exceeded");
    out.push_back(p);
  }
  void flush(Flow &f) {
    if (f.started)
      f.y += f.line_height;
    f.x = f.left;
    f.line_height = 0;
    f.started = false;
    f.pending_space = false;
  }
  int width(const Font &font, const std::string &text) {
    int n = measure(context, font, text);
    if (n < 0 || n > 1000000)
      throw std::runtime_error("invalid text measurement");
    return n;
  }
  void piece(Flow &f, const Style &s, const std::string &text,
             const std::string &href) {
    int w = width(s.font, text);
    if (f.started && f.x + w > f.right)
      flush(f);
    Paint p = {
        Paint::Text, f.x,     f.y,  w,    s.font.pixels + s.font.pixels / 4 + 2,
        s.font,      s.color, text, href, s.underline};
    emit(p);
    f.x += w;
    f.line_height = std::max(f.line_height, p.height);
    f.started = true;
  }
  void word(Flow &f, const Style &s, const std::string &text,
            const std::string &href) {
    int w = width(s.font, text),
        blank = f.pending_space && f.started ? width(s.font, " ") : 0;
    if (f.started && f.x + blank + w > f.right)
      flush(f);
    else if (blank)
      piece(f, s, " ", href);
    f.pending_space = false;
    if (w <= f.right - f.left) {
      piece(f, s, text, href);
      return;
    }
    std::string part;
    for (size_t i = 0; i < text.size();) {
      size_t end = i + 1;
      while (end < text.size() &&
             (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
        end++;
      std::string next = part + text.substr(i, end - i);
      if (!part.empty() && f.x + width(s.font, next) > f.right) {
        piece(f, s, part, href);
        flush(f);
        part.clear();
      }
      part += text.substr(i, end - i);
      i = end;
      if (width(s.font, part) > f.right - f.left) {
        piece(f, s, part, href);
        flush(f);
        part.clear();
      }
    }
    if (!part.empty())
      piece(f, s, part, href);
  }
  void text(Flow &f, const Style &s, const std::string &content,
            const std::string &href) {
    size_t i = 0;
    while (i < content.size()) {
      if (space(content[i])) {
        f.pending_space = true;
        i++;
        continue;
      }
      size_t end = i + 1;
      while (end < content.size() && !space(content[end]))
        end++;
      word(f, s, content.substr(i, end - i), href);
      i = end;
    }
  }
  void node(size_t index, Flow &flow, const Style &parent,
            const std::string &link) {
    const Node &n = tree.nodes[index];
    if (n.tag == "#text") {
      text(flow, parent, n.text, link);
      return;
    }
    Style s = computed(n, parent, tree.rules);
    if (s.hidden)
      return;
    if (n.tag == "br") {
      if (!flow.started) {
        flow.started = true;
        flow.line_height = s.font.pixels + s.font.pixels / 4 + 2;
      }
      flush(flow);
      return;
    }
    std::string href = n.tag == "a" ? safe_href(n.href) : link;
    if (s.block) {
      flush(flow);
      int left = std::min(flow.right - 1, flow.left + s.margin),
          top = flow.y + s.margin;
      size_t background = out.size();
      if (s.has_background) {
        Paint box = {Paint::Background,
                     left,
                     top,
                     std::max(1, flow.right - left - s.margin),
                     0,
                     s.font,
                     s.background,
                     "",
                     "",
                     false};
        emit(box);
      }
      Flow inner = {
          left + s.padding,
          std::max(left + s.padding + 1, flow.right - s.margin - s.padding),
          left + s.padding,
          top + s.padding,
          0,
          false,
          false};
      if (n.tag == "li")
        text(inner, s, "- ", href);
      for (size_t child : n.children)
        node(child, inner, s, href);
      flush(inner);
      int bottom = inner.y + s.padding;
      if (s.has_background)
        out[background].height = std::max(1, bottom - top);
      flow.y = bottom + s.margin;
    } else
      for (size_t child : n.children)
        node(child, flow, s, href);
    if (flow.y > 1000000)
      throw std::length_error("document height limit exceeded");
  }

public:
  Layout(const Tree &t, Measure m, void *c, std::vector<Paint> &p)
      : tree(t), measure(m), context(c), out(p) {}
  int run(int w) {
    Style defaults = {
        {16, false, false}, 0x202020, 0, false, false, true, false, 0, 0};
    Flow f = {0, w, 0, 0, 0, false, false};
    node(0, f, defaults, "");
    flush(f);
    return f.y;
  }
};
} // namespace
SubsetDocument::SubsetDocument() : complete_(false), height_(0) {}
bool SubsetDocument::append(const char *bytes, size_t length,
                            bool final_chunk) {
  if (complete_) {
    error_ = "document already complete; reset before reuse";
    return false;
  }
  if ((!bytes && length) || length > input_limit - html_.size()) {
    error_ = "HTML input limit exceeded or null input";
    return false;
  }
  if (length)
    html_.append(bytes, length);
  complete_ = final_chunk;
  error_.clear();
  return true;
}
void SubsetDocument::reset() {
  html_.clear();
  paints_.clear();
  error_.clear();
  height_ = 0;
  complete_ = false;
}
bool SubsetDocument::layout(int w, Measure m, void *c) {
  if (w < 1 || w > 32768 || !m) {
    error_ = "invalid viewport or measurement callback";
    return false;
  }
  try {
    Tree tree = parse(html_, complete_);
    std::vector<Paint> next;
    Layout engine(tree, m, c, next);
    int h = engine.run(w);
    paints_.swap(next);
    height_ = h;
    error_.clear();
    return true;
  } catch (const std::exception &e) {
    error_ = e.what();
    return false;
  }
}
std::string SubsetDocument::link_at(int x, int y) const {
  for (auto i = paints_.rbegin(); i != paints_.rend(); ++i)
    if (i->kind == Paint::Text && !i->href.empty() && x >= i->x &&
        x < i->x + i->width && y >= i->y && y < i->y + i->height)
      return i->href;
  return std::string();
}
} // namespace iewk
