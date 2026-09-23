#include "subset.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
static int measure(void *, const iewk::Font &font, const std::string &text) {
  int count = 0;
  for (unsigned char c : text)
    if ((c & 0xc0) != 0x80)
      count += c >= 0xe0 ? 5 : 3; // Approximate wide CJK glyphs in the demo.
  return count * font.pixels / 5;
}
static std::string escape(const std::string &s) {
  std::string out;
  for (unsigned char c : s) {
    if (c == '&')
      out += "&amp;";
    else if (c == '<')
      out += "&lt;";
    else if (c == '>')
      out += "&gt;";
    else if (c == '\"')
      out += "&quot;";
    else if (c < 32)
      out += ' ';
    else
      out += static_cast<char>(c);
  }
  return out;
}
int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: preview input.html output.svg\n";
    return 2;
  }
  std::ifstream input(argv[1], std::ios::binary);
  if (!input)
    return 2;
  iewk::SubsetDocument doc;
  char chunk[257];
  while (input) {
    input.read(chunk, sizeof chunk);
    std::streamsize n = input.gcount();
    if (n && !doc.append(chunk, static_cast<size_t>(n)))
      return 3;
  }
  if (!doc.append(NULL, 0, true) || !doc.layout(640, measure, NULL)) {
    std::cerr << doc.error() << '\n';
    return 3;
  }
  std::ofstream out(argv[2]);
  if (!out)
    return 2;
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"640\" height=\""
      << doc.height() << "\" viewBox=\"0 0 640 " << doc.height()
      << "\"><rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
  for (const auto &p : doc.paint()) {
    out << (p.kind == iewk::Paint::Background ? "<rect" : "<text") << " x=\""
        << p.x << "\" y=\""
        << (p.kind == iewk::Paint::Background ? p.y : p.y + p.font.pixels)
        << "\" fill=\"#" << std::hex << std::setw(6) << std::setfill('0')
        << p.color << std::dec << "\"";
    if (p.kind == iewk::Paint::Background)
      out << " width=\"" << p.width << "\" height=\"" << p.height << "\"/>\n";
    else
      out << " font-family=\"monospace\" font-size=\"" << p.font.pixels
          << "\" font-weight=\"" << (p.font.bold ? "bold" : "normal")
          << "\" font-style=\"" << (p.font.italic ? "italic" : "normal")
          << "\" text-decoration=\"" << (p.underline ? "underline" : "none")
          << "\">" << escape(p.text) << "</text>\n";
  }
  out << "</svg>\n";
  return out ? 0 : 2;
}
