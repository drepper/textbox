#include "textbox.hh"

#include <cassert>
#include <cstdio>
#include <format>
#include <string_view>

#include <sys/uio.h>
#include <unistd.h>

#include <unistr.h>
#include <uniwidth.h>

namespace widget {

namespace {

/// Helper to write string to terminal
inline void write_str(int fd, std::string_view str) {
  ::write(fd, str.data(), str.size());
}

/// Helper to write multiple strings to terminal using writev
inline void writev_strs(int fd, std::initializer_list<std::string_view> strs) {
  std::vector<iovec> iov;
  iov.resize(strs.size());
  size_t count = 0;

  for (const auto &str : strs)
    if (!str.empty()) {
      iov[count].iov_base = const_cast<char *>(str.data());
      iov[count].iov_len = str.size();
      ++count;
    }

  if (count > 0)
    ::writev(fd, iov.data(), count);
}

/// Parse escape sequences and calculate display width
struct text_metrics {
  unsigned display_width = 0;
  size_t byte_length = 0;
};

/// Get display width accounting for UTF-8 and escape sequences
text_metrics measure_text(const std::string &text,
                          unsigned max_width = UINT_MAX) {
  text_metrics result;
  const char *p = text.data();
  const char *end = p + text.size();

  while (p < end && result.display_width < max_width) {
    // Check for escape sequences
    if (*p == '\e') {
      const char *seq_start = p;
      ++p;
      if (p < end && *p == '[') {
        // CSI sequence
        ++p;
        while (p < end && (*p == ';' || (*p >= '0' && *p <= '9') ||
                           (*p >= '<' && *p <= '?')))
          ++p;
        if (p < end && (*p >= '@' && *p <= '~'))
          ++p;
      } else if (p < end && *p == ']') {
        // OSC sequence
        ++p;
        while (p < end && *p != '\a' &&
               !(*p == '\e' && p + 1 < end && *(p + 1) == '\\'))
          ++p;
        if (p < end && *p == '\a')
          ++p;
        else if (p + 1 < end && *p == '\e' && *(p + 1) == '\\')
          p += 2;
      } else if (p < end &&
                 (*p == '(' || *p == ')' || *p == '*' || *p == '+')) {
        // Character set sequences
        ++p;
        if (p < end)
          ++p;
      }
      result.byte_length += p - seq_start;
      continue;
    }

    // Regular UTF-8 character
    ucs4_t uc;
    const uint8_t *u8p = reinterpret_cast<const uint8_t *>(p);
    int len = ::u8_mbtouc(&uc, u8p, end - p);
    if (len <= 0) [[unlikely]] {
      ++p;
      ++result.byte_length;
      continue;
    }

    int width = ::uc_width(uc, "UTF-8");
    if (width < 0)
      width = 1;

    if (result.display_width + width > max_width)
      break;

    result.display_width += width;
    result.byte_length += len;
    p += len;
  }

  return result;
}

} // anonymous namespace

textbox::textbox(const terminal::info &term, const std::string &name)
    : term_info{term}, widget_name{name}, title{name} {
  // Move to column 1 to ensure clean starting position
  write_str(term_info.get_fd(), "\r");

  // Initialize with one empty paragraph
  paragraphs.emplace_back();

  // Initialize title foreground from terminal default
  title_fg = term_info.default_foreground;
}

textbox::~textbox() {
  // Cursor is already positioned at the line after widget (from last newline)
  // No action needed
}

void textbox::set_title(const std::string &new_title) {
  title = new_title;
  if (has_been_drawn)
    render();
}

void textbox::add_text(const std::string &text) {
  for (char ch : text) {
    if (ch == '\n') {
      // Close current paragraph if not empty, start new one
      if (!paragraphs.back().content.empty()) {
        assert(paragraphs.back().is_reflow);
        paragraphs.emplace_back();
      }
    } else if (ch != '\r')
      // Append character to current paragraph
      paragraphs.back().content += ch;
  }

  render();
}

void textbox::add_block(const std::vector<std::string> &lines) {
  std::string block_content;
  for (const auto &line : lines) {
    block_content += line;
    if (block_content.empty() || block_content.back() != '\n')
      block_content += '\n';
  }

  // If current paragraph is empty, insert before it
  if (paragraphs.back().content.empty()) {
    paragraphs.insert(paragraphs.end() - 1, paragraph{block_content, false});
  } else {
    // Close current paragraph and add new fixed paragraph
    paragraphs.push_back(paragraph{block_content, false});
    // Ensure there's always an empty paragraph at the end
    paragraphs.push_back(paragraph{});
  }

  render();
}

void textbox::set_frame(frame_type ft) {
  frame = ft;
  if (has_been_drawn)
    render();
}

void textbox::set_left_margin(unsigned margin) {
  left_margin = margin;
  if (has_been_drawn)
    render();
}

void textbox::set_right_margin(unsigned margin) {
  right_margin = margin;
  if (has_been_drawn)
    render();
}

void textbox::set_text_foreground(uint8_t r, uint8_t g, uint8_t b) {
  text_fg = {r, g, b};
  if (has_been_drawn)
    render();
}

void textbox::set_text_background(uint8_t r, uint8_t g, uint8_t b) {
  text_bg = {r, g, b};
  if (has_been_drawn)
    render();
}

void textbox::set_frame_foreground(uint8_t r, uint8_t g, uint8_t b) {
  frame_fg = {r, g, b};
  if (has_been_drawn)
    render();
}

void textbox::draw() { render(); }

void textbox::render() {
  auto [term_height, term_width] = get_terminal_dimensions();
  unsigned content_width = term_width - left_margin - right_margin;

  // Account for frame borders
  if (frame != frame_type::none)
    content_width -= 2;

  int fd = term_info.get_fd();

  // If re-rendering, move cursor back to start of widget
  if (has_been_drawn) {
    move_cursor_up(widget_height);
    write_str(fd, "\r"); // Move to column 1
  }

  // Clear to end of line when re-rendering to remove artifacts
  std::string clear_eol = has_been_drawn ? "\e[K" : "";

  // Calculate total widget height for next re-render
  unsigned new_height = 0;

  // Top frame
  if (frame != frame_type::none)
    ++new_height;

  // Count content lines for height calculation
  for (size_t i = 0; i < paragraphs.size(); ++i) {
    const auto &para = paragraphs[i];

    if (para.content.empty())
      continue;

    if (para.is_reflow) {
      auto lines = wrap_paragraph(para.content, content_width);
      new_height += lines.size();
    } else {
      // Count lines in fixed paragraph
      std::string::size_type pos = 0;
      while (pos < para.content.size()) {
        auto newline_pos = para.content.find('\n', pos);
        if (newline_pos == std::string::npos)
          newline_pos = para.content.size();
        ++new_height;
        pos = newline_pos + 1;
      }
    }

    // Empty line between paragraphs
    if (i + 1 < paragraphs.size() && !paragraphs[i + 1].content.empty())
      ++new_height;
  }

  // Bottom frame
  if (frame != frame_type::none)
    ++new_height;

  // Create left margin string
  std::string left_margin_spaces(left_margin, ' ');

  // Render top frame
  if (frame == frame_type::line) {
    if (!title.empty()) {
      auto metrics = measure_text(title, content_width);
      unsigned remaining = content_width - metrics.display_width;
      std::string horiz_line(remaining * 3, '\0');
      for (unsigned i = 0; i < remaining; ++i) {
        horiz_line[i * 3] = '\xe2';
        horiz_line[i * 3 + 1] = '\x94';
        horiz_line[i * 3 + 2] = '\x80';
      }
      writev_strs(fd,
                  {left_margin_spaces, color_escape(frame_fg, true),
                   "\N{BOX DRAWINGS LIGHT ARC DOWN AND RIGHT}",
                   color_escape(title_fg, true), color_escape(text_bg, false),
                   title.substr(0, metrics.byte_length), "\e[0m",
                   color_escape(frame_fg, true), horiz_line,
                   "\N{BOX DRAWINGS LIGHT ARC DOWN AND LEFT}\e[0m", clear_eol,
                   "\n"});
    } else {
      std::string horiz_line(content_width * 3, '\0');
      for (unsigned i = 0; i < content_width; ++i) {
        horiz_line[i * 3] = '\xe2';
        horiz_line[i * 3 + 1] = '\x94';
        horiz_line[i * 3 + 2] = '\x80';
      }
      writev_strs(fd, {left_margin_spaces, color_escape(frame_fg, true),
                       "\N{BOX DRAWINGS LIGHT ARC DOWN AND RIGHT}", horiz_line,
                       "\N{BOX DRAWINGS LIGHT ARC DOWN AND LEFT}\e[0m",
                       clear_eol, "\n"});
    }
  } else if (frame == frame_type::background) {
    if (!title.empty()) {
      auto metrics = measure_text(title, content_width);
      unsigned remaining = content_width - metrics.display_width;
      std::string lower_half(remaining * 3, '\0');
      for (unsigned i = 0; i < remaining; ++i) {
        lower_half[i * 3] = '\xe2';
        lower_half[i * 3 + 1] = '\x96';
        lower_half[i * 3 + 2] = '\x84';
      }
      writev_strs(fd, {left_margin_spaces, color_escape(frame_fg, true),
                       "\N{QUADRANT LOWER RIGHT}", color_escape(title_fg, true),
                       color_escape(text_bg, false),
                       title.substr(0, metrics.byte_length), "\e[0m",
                       color_escape(frame_fg, true), lower_half,
                       "\N{QUADRANT LOWER LEFT}\e[0m", clear_eol, "\n"});
    } else {
      std::string lower_half(content_width * 3, '\0');
      for (unsigned i = 0; i < content_width; ++i) {
        lower_half[i * 3] = '\xe2';
        lower_half[i * 3 + 1] = '\x96';
        lower_half[i * 3 + 2] = '\x84';
      }
      writev_strs(fd, {left_margin_spaces, color_escape(frame_fg, true),
                       "\N{QUADRANT LOWER RIGHT}", lower_half,
                       "\N{QUADRANT LOWER LEFT}\e[0m", clear_eol, "\n"});
    }
  }

  // Render paragraphs
  std::string frame_color = color_escape(frame_fg, true);
  std::string text_color = color_escape(text_fg, true);
  std::string bg_color =
      (frame == frame_type::background) ? color_escape(text_bg, false) : "";

  for (size_t i = 0; i < paragraphs.size(); ++i) {
    const auto &para = paragraphs[i];

    if (para.content.empty())
      continue;

    if (para.is_reflow) {
      // Wrap paragraph
      auto lines = wrap_paragraph(para.content, content_width);
      for (const auto &line : lines) {
        if (frame == frame_type::line)
          writev_strs(fd, {left_margin_spaces, frame_color,
                           "\N{BOX DRAWINGS LIGHT VERTICAL}", text_color, line,
                           frame_color, "\N{BOX DRAWINGS LIGHT VERTICAL}\e[0m",
                           clear_eol, "\n"});
        else if (frame == frame_type::background)
          writev_strs(fd,
                      {left_margin_spaces, frame_color, "\N{RIGHT HALF BLOCK}",
                       bg_color, text_color, line, "\e[0m", frame_color,
                       "\N{LEFT HALF BLOCK}\e[0m", clear_eol, "\n"});
        else
          writev_strs(fd, {left_margin_spaces, text_color, bg_color, line,
                           "\e[0m", clear_eol, "\n"});
      }
    } else {
      // Fixed paragraph - render lines as-is, truncating if needed
      std::string::size_type pos = 0;
      while (pos < para.content.size()) {
        auto newline_pos = para.content.find('\n', pos);
        if (newline_pos == std::string::npos)
          newline_pos = para.content.size();

        std::string line = para.content.substr(pos, newline_pos - pos);
        line = truncate_text(line, content_width);

        if (frame == frame_type::line)
          writev_strs(fd, {left_margin_spaces, frame_color,
                           "\N{BOX DRAWINGS LIGHT VERTICAL}", text_color, line,
                           frame_color, "\N{BOX DRAWINGS LIGHT VERTICAL}\e[0m",
                           clear_eol, "\n"});
        else if (frame == frame_type::background)
          writev_strs(fd,
                      {left_margin_spaces, frame_color, "\N{RIGHT HALF BLOCK}",
                       bg_color, text_color, line, "\e[0m", frame_color,
                       "\N{LEFT HALF BLOCK}\e[0m", clear_eol, "\n"});
        else
          writev_strs(fd, {left_margin_spaces, text_color, bg_color, line,
                           "\e[0m", clear_eol, "\n"});

        pos = newline_pos + 1;
      }
    }

    // Add empty line between paragraphs (except for last empty paragraph)
    if (i + 1 < paragraphs.size() && !paragraphs[i + 1].content.empty()) {
      std::string spaces = std::string(content_width, ' ');
      if (frame == frame_type::line)
        writev_strs(fd,
                    {left_margin_spaces, frame_color,
                     "\N{BOX DRAWINGS LIGHT VERTICAL}", spaces,
                     "\N{BOX DRAWINGS LIGHT VERTICAL}\e[0m", clear_eol, "\n"});
      else if (frame == frame_type::background)
        writev_strs(fd,
                    {left_margin_spaces, frame_color, "\N{RIGHT HALF BLOCK}",
                     bg_color, spaces, "\e[0m", frame_color,
                     "\N{LEFT HALF BLOCK}\e[0m", clear_eol, "\n"});
      else
        writev_strs(fd, {left_margin_spaces, bg_color, clear_eol, "\n"});
    }
  }

  // Render bottom frame
  if (frame == frame_type::line) {
    std::string horiz_line(content_width * 3, '\0');
    for (unsigned i = 0; i < content_width; ++i) {
      horiz_line[i * 3] = '\xe2';
      horiz_line[i * 3 + 1] = '\x94';
      horiz_line[i * 3 + 2] = '\x80';
    }
    writev_strs(fd, {left_margin_spaces, frame_color,
                     "\N{BOX DRAWINGS LIGHT ARC UP AND RIGHT}", horiz_line,
                     "\N{BOX DRAWINGS LIGHT ARC UP AND LEFT}\e[0m", clear_eol,
                     "\n"});
  } else if (frame == frame_type::background) {
    std::string upper_half(content_width * 3, '\0');
    for (unsigned i = 0; i < content_width; ++i) {
      upper_half[i * 3] = '\xe2';
      upper_half[i * 3 + 1] = '\x96';
      upper_half[i * 3 + 2] = '\x80';
    }
    writev_strs(fd,
                {left_margin_spaces, frame_color, "\N{QUADRANT UPPER RIGHT}",
                 upper_half, "\N{QUADRANT UPPER LEFT}\e[0m", clear_eol, "\n"});
  }

  // Update height for next re-render
  widget_height = new_height;

  // Mark as drawn
  has_been_drawn = true;
}

unsigned textbox::calculate_display_width(const std::string &text) const {
  return measure_text(text).display_width;
}

std::vector<std::string> textbox::wrap_paragraph(const std::string &text,
                                                 unsigned width) const {
  std::vector<std::string> lines;
  if (text.empty())
    return lines;

  size_t pos = 0;
  while (pos < text.size()) {
    auto metrics = measure_text(text.substr(pos), width);

    if (metrics.byte_length == 0) {
      // Handle case where even one character is too wide
      size_t next = pos + 1;
      while (next < text.size() && (text[next] & 0xC0) == 0x80)
        ++next;
      lines.push_back(text.substr(pos, next - pos));
      pos = next;
    } else {
      // Find good break point
      size_t break_point = pos + metrics.byte_length;

      // Try to break at space if we're not at end of text
      if (break_point < text.size()) {
        size_t last_space = text.rfind(' ', break_point);
        if (last_space != std::string::npos && last_space > pos)
          break_point = last_space + 1;
      }

      std::string line = text.substr(pos, break_point - pos);
      // Pad line to width
      unsigned line_width = calculate_display_width(line);
      if (line_width < width)
        line += std::string(width - line_width, ' ');

      lines.push_back(line);
      pos = break_point;

      // Skip leading spaces on next line
      while (pos < text.size() && text[pos] == ' ')
        ++pos;
    }
  }

  return lines;
}

std::string textbox::truncate_text(const std::string &text,
                                   unsigned width) const {
  auto metrics = measure_text(text, width);
  std::string result = text.substr(0, metrics.byte_length);

  // Pad to width
  if (metrics.display_width < width)
    result += std::string(width - metrics.display_width, ' ');

  return result;
}

std::tuple<unsigned, unsigned> textbox::get_terminal_dimensions() {
  return term_info.get_geometry().value_or(std::tuple{24u, 80u});
}

void textbox::move_cursor_up(unsigned lines) const {
  if (lines > 0) {
    std::string seq = std::format("\e[{}A", lines);
    write_str(term_info.get_fd(), seq);
  }
}

std::string textbox::color_escape(const terminal::info::color &color,
                                  bool foreground) const {
  return std::format("\e[{};2;{};{};{}m", foreground ? "38" : "48", color.r,
                     color.g, color.b);
}

} // namespace widget
