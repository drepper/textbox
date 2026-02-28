#include "textbox.hh"

#include <cassert>
#include <cstdio>
#include <format>
#include <sstream>
#include <string_view>

#include <sys/uio.h>
#include <unistd.h>

#include <unistr.h>
#include <uniwidth.h>

#include <srchilite/sourcehighlight.h>
#include <srchilite/langmap.h>

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

void textbox::add_markdown(const std::string &markdown) {
  raw_markdown += markdown;
  parse_markdown();
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

void textbox::parse_markdown() {
  // Clear all paragraphs
  paragraphs.clear();
  paragraphs.emplace_back(); // Start with empty paragraph

  // Reset heading counters
  for (unsigned &counter : heading_counters)
    counter = 0;

  if (raw_markdown.empty())
    return;

  size_t pos = 0;
  std::string current_para;
  bool at_line_start = true;

  while (pos < raw_markdown.size()) {
    // Check for heading at start of line
    if (at_line_start && raw_markdown[pos] == '#') {
      // Count heading level
      size_t heading_level = 0;
      size_t hash_pos = pos;
      while (hash_pos < raw_markdown.size() && raw_markdown[hash_pos] == '#' &&
             heading_level < 6) {
        ++heading_level;
        ++hash_pos;
      }

      // Check for space after hashes
      if (hash_pos < raw_markdown.size() &&
          (raw_markdown[hash_pos] == ' ' || raw_markdown[hash_pos] == '\t')) {
        // Extract heading text until newline
        ++hash_pos; // Skip space
        size_t heading_start = hash_pos;
        size_t heading_end = raw_markdown.find('\n', heading_start);
        if (heading_end == std::string::npos)
          heading_end = raw_markdown.size();

        std::string heading_text =
            raw_markdown.substr(heading_start, heading_end - heading_start);

        // Trim trailing whitespace
        size_t last = heading_text.find_last_not_of(" \t\r");
        if (last != std::string::npos)
          heading_text = heading_text.substr(0, last + 1);

        // Update heading counters - increment current level, reset lower levels
        ++heading_counters[heading_level - 1];
        for (size_t i = heading_level; i < 6; ++i)
          heading_counters[i] = 0;

        // Build hierarchical numbering prefix: ¶ 1.2.3  heading text
        std::string numbering = "\N{PILCROW SIGN} ";
        for (size_t i = 0; i < heading_level; ++i) {
          if (i > 0)
            numbering += '.';
          numbering += std::to_string(heading_counters[i]);
        }
        numbering += "  ";

        // Save any pending paragraph
        if (!current_para.empty()) {
          paragraphs.back().content = current_para;
          current_para.clear();
          paragraphs.emplace_back();
        }

        // Build heading with appropriate color based on level
        std::string formatted_heading;
        switch (heading_level) {
        case 1:
          formatted_heading = color_escape(h1_fg, true) + numbering +
                              heading_text + "\e[0m";
          break;
        case 2:
          formatted_heading = color_escape(h2_fg, true) + numbering +
                              heading_text + "\e[0m";
          break;
        case 3:
          formatted_heading = color_escape(h3_fg, true) + numbering +
                              heading_text + "\e[0m";
          break;
        case 4:
          formatted_heading = color_escape(h4_fg, true) + numbering +
                              heading_text + "\e[0m";
          break;
        case 5:
          formatted_heading = color_escape(h5_fg, true) + numbering +
                              heading_text + "\e[0m";
          break;
        case 6:
          formatted_heading = color_escape(h6_fg, true) + numbering +
                              heading_text + "\e[0m";
          break;
        default:
          formatted_heading = heading_text;
          break;
        }

        // Save any pending paragraph content first
        if (!current_para.empty()) {
          if (!paragraphs.back().content.empty())
            paragraphs.emplace_back();
          paragraphs.back().content = current_para;
          current_para.clear();
          paragraphs.emplace_back(); // Create new empty paragraph for heading
        }

        // Add heading as new paragraph (always fixed to preserve formatting)
        if (!paragraphs.back().content.empty())
          paragraphs.emplace_back();
        paragraphs.back().content = formatted_heading;
        paragraphs.back().is_reflow = false;

        // Move past heading and its newline
        pos = heading_end;
        if (pos < raw_markdown.size() && raw_markdown[pos] == '\n') {
          ++pos;
          at_line_start = true;
        }

        continue;
      }
    }

    // Check for code block (only at line start)
    if (at_line_start && pos + 3 <= raw_markdown.size() &&
        raw_markdown.substr(pos, 3) == "```") {
      // Save any pending paragraph content
      if (!current_para.empty()) {
        if (!paragraphs.back().content.empty())
          paragraphs.emplace_back();
        paragraphs.back().content = current_para;
        current_para.clear();
        paragraphs.emplace_back(); // Create new empty paragraph for code block
      }

      // Find end of opening ```
      pos += 3;
      size_t lang_start = pos;
      size_t lang_end = raw_markdown.find('\n', pos);
      if (lang_end == std::string::npos)
        break;

      std::string language = raw_markdown.substr(lang_start, lang_end - lang_start);
      // Trim whitespace
      size_t first = language.find_first_not_of(" \t\r");
      size_t last = language.find_last_not_of(" \t\r");
      if (first != std::string::npos)
        language = language.substr(first, last - first + 1);

      pos = lang_end + 1;

      // Find closing ```
      size_t code_end = raw_markdown.find("\n```", pos);
      if (code_end == std::string::npos)
        code_end = raw_markdown.find("```", pos);
      if (code_end == std::string::npos)
        code_end = raw_markdown.size();

      std::string code = raw_markdown.substr(pos, code_end - pos);

      // Use source-highlight library if language is specified
      std::string highlighted_code;
      bool has_highlighting = false;
      if (!language.empty()) {
        try {
          // Create source-highlight instance
          srchilite::SourceHighlight highlighter{"esc.outlang"};

          // Use input/output streams
          std::istringstream input{code};
          std::ostringstream output;

          // Highlight the code
          highlighter.highlight(input, output, language);
          highlighted_code = output.str();
          has_highlighting = true;
        } catch (...) {
          // If highlighting fails, fall through to default
        }
      }

      // If source-highlight failed or no language, use default code style
      if (highlighted_code.empty())
        highlighted_code = code;

      // Build the complete code block with indentation and optional header
      std::string code_block;
      std::string bg_escape = color_escape(code_block_bg, false);

      // Add language header if language is specified
      if (!language.empty()) {
        code_block += color_escape(code_fg, true) +
                      color_escape(code_lang_bg, false) + "    [" + language +
                      "]\e[0m\n";
      }

      // For syntax-highlighted code, replace reset sequences to preserve background
      if (has_highlighting) {
        // Replace \e[0m and \e[m with \e[39m (reset foreground only) + background
        std::string replacement = "\e[39m" + bg_escape;
        size_t pos_replace = 0;
        while ((pos_replace = highlighted_code.find("\e[0m", pos_replace)) !=
               std::string::npos) {
          highlighted_code.replace(pos_replace, 4, replacement);
          pos_replace += replacement.length();
        }
        pos_replace = 0;
        while ((pos_replace = highlighted_code.find("\e[m", pos_replace)) !=
               std::string::npos) {
          highlighted_code.replace(pos_replace, 3, replacement);
          pos_replace += replacement.length();
        }
      }

      // Split code into lines and indent each line by 4 spaces
      std::istringstream code_stream{highlighted_code};
      std::string line;
      while (::std::getline(code_stream, line)) {
        // Apply background color and 4-space indentation
        code_block += bg_escape + "    ";

        // If no syntax highlighting (plain code), add foreground color
        if (!has_highlighting)
          code_block += color_escape(code_fg, true);

        code_block += line + "\e[0m\n";
      }

      // Add as fixed paragraph
      if (!paragraphs.back().content.empty())
        paragraphs.emplace_back();
      paragraphs.back().content = code_block;
      paragraphs.back().is_reflow = false;

      // Skip past closing ```
      pos = code_end;
      if (pos < raw_markdown.size() && raw_markdown[pos] == '\n')
        ++pos;
      if (pos + 3 <= raw_markdown.size() &&
          raw_markdown.substr(pos, 3) == "```")
        pos += 3;
      if (pos < raw_markdown.size() && raw_markdown[pos] == '\n') {
        ++pos;
        at_line_start = true;
      }

      continue;
    }

    // Non-heading, non-code-block character - no longer at line start
    at_line_start = false;

    // Process regular text with inline formatting
    char ch = raw_markdown[pos];

    if (ch == '\n') {
      ++pos;
      at_line_start = true;
      // Check for paragraph break (double newline)
      if (pos < raw_markdown.size() && raw_markdown[pos] == '\n') {
        // Paragraph break
        if (!current_para.empty()) {
          if (!paragraphs.back().content.empty())
            paragraphs.emplace_back();
          paragraphs.back().content = current_para;
          current_para.clear();
          paragraphs.emplace_back(); // Create new empty paragraph
        }
        ++pos; // Skip second newline
      } else if (!current_para.empty()) {
        // Single newline becomes space in reflowed text (if we're in a paragraph)
        current_para += ' ';
      }
      continue;
    }

    // Skip leading whitespace at line start (but not in middle of paragraph)
    if (at_line_start && (ch == ' ' || ch == '\t')) {
      ++pos;
      continue;
    }

    // Check for inline code `...`
    if (ch == '`') {
      size_t end = raw_markdown.find('`', pos + 1);
      if (end != std::string::npos) {
        std::string code_text = raw_markdown.substr(pos + 1, end - pos - 1);
        current_para += color_escape(code_fg, true) +
                        color_escape(code_bg, false) + code_text + "\e[0m";
        pos = end + 1;
        continue;
      }
    }

    // Check for bold **...**
    if (pos + 1 < raw_markdown.size() && ch == '*' &&
        raw_markdown[pos + 1] == '*') {
      size_t end = raw_markdown.find("**", pos + 2);
      if (end != std::string::npos) {
        std::string bold_text = raw_markdown.substr(pos + 2, end - pos - 2);
        current_para += "\e[1m" + color_escape(bold_fg, true) + bold_text +
                        "\e[22m\e[0m";
        pos = end + 2;
        continue;
      }
    }

    // Check for italic *...*
    if (ch == '*') {
      size_t end = raw_markdown.find('*', pos + 1);
      if (end != std::string::npos) {
        std::string italic_text = raw_markdown.substr(pos + 1, end - pos - 1);
        current_para += "\e[3m" + color_escape(italic_fg, true) +
                        italic_text + "\e[23m\e[0m";
        pos = end + 1;
        continue;
      }
    }

    // Check for strikethrough ~~...~~
    if (pos + 1 < raw_markdown.size() && ch == '~' &&
        raw_markdown[pos + 1] == '~') {
      size_t end = raw_markdown.find("~~", pos + 2);
      if (end != std::string::npos) {
        std::string strike_text = raw_markdown.substr(pos + 2, end - pos - 2);
        current_para += "\e[9m" + color_escape(strikethrough_fg, true) +
                        strike_text + "\e[29m\e[0m";
        pos = end + 2;
        continue;
      }
    }

    // Regular character
    current_para += ch;
    ++pos;
  }

  // Add final paragraph if not empty
  if (!current_para.empty()) {
    if (!paragraphs.back().content.empty())
      paragraphs.emplace_back();
    paragraphs.back().content = current_para;
  }

  // Ensure there's always an empty paragraph at the end
  if (paragraphs.empty() || !paragraphs.back().content.empty())
    paragraphs.emplace_back();

  // Remove any empty paragraphs except the last one
  if (paragraphs.size() > 1) {
    auto it = paragraphs.begin();
    while (it != paragraphs.end() - 1) {
      if (it->content.empty())
        it = paragraphs.erase(it);
      else
        ++it;
    }
  }
}

void textbox::render() {
  auto [term_width, term_height] = get_terminal_dimensions();

  // Calculate minimum width needed
  unsigned min_width = left_margin + right_margin;
  if (frame != frame_type::none)
    min_width += 2;

  // Ensure terminal is wide enough
  if (term_width < min_width)
    return; // Can't render - terminal too narrow

  unsigned content_width = term_width - left_margin - right_margin;

  // Account for frame borders
  if (frame != frame_type::none)
    content_width -= 2;

  int fd = term_info.get_fd();

  // Clear to end of line when re-rendering to remove artifacts
  std::string clear_eol = has_been_drawn && right_margin > 0 ? "\e[K" : "";

  // Create left margin string
  std::string left_margin_spaces(left_margin, ' ');

  // Calculate total content lines needed FIRST (before moving cursor)
  std::vector<std::string> all_lines;

  for (size_t i = 0; i < paragraphs.size(); ++i) {
    const auto &para = paragraphs[i];

    if (para.content.empty())
      continue;

    if (para.is_reflow) {
      auto lines = wrap_paragraph(para.content, content_width);
      all_lines.insert(all_lines.end(), lines.begin(), lines.end());
    } else {
      // Fixed paragraph - split into lines
      std::string::size_type pos = 0;
      while (pos < para.content.size()) {
        auto newline_pos = para.content.find('\n', pos);
        if (newline_pos == std::string::npos)
          newline_pos = para.content.size();
        std::string line = para.content.substr(pos, newline_pos - pos);
        all_lines.push_back(truncate_text(line, content_width));
        pos = newline_pos + 1;
      }
    }

    // Empty line between paragraphs
    if (i + 1 < paragraphs.size() && !paragraphs[i + 1].content.empty())
      all_lines.push_back(std::string(content_width, ' '));
  }

  // Calculate total widget height
  unsigned new_height = all_lines.size();
  if (frame != frame_type::none)
    new_height += 2; // Top and bottom frame

  // NOW move cursor back to start of widget (using OLD height)
  if (has_been_drawn) {
    move_cursor_up(widget_height);
    write_str(fd, "\r"); // Move to column 1
  }

  // Step 1: Draw the frame structure
  std::string frame_color = color_escape(frame_fg, true);
  std::string text_color = color_escape(text_fg, true);
  std::string bg_color =
      (frame == frame_type::background) ? color_escape(text_bg, false) : "";
  std::string content_spaces(content_width, ' ');

  if (frame == frame_type::line) {
    // Draw top frame
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
                  {left_margin_spaces, frame_color,
                   "\N{BOX DRAWINGS LIGHT ARC DOWN AND RIGHT}",
                   color_escape(title_fg, true), color_escape(text_bg, false),
                   title.substr(0, metrics.byte_length), "\e[0m", frame_color,
                   horiz_line, "\N{BOX DRAWINGS LIGHT ARC DOWN AND LEFT}\e[0m",
                   clear_eol, "\n"});
    } else {
      std::string horiz_line(content_width * 3, '\0');
      for (unsigned i = 0; i < content_width; ++i) {
        horiz_line[i * 3] = '\xe2';
        horiz_line[i * 3 + 1] = '\x94';
        horiz_line[i * 3 + 2] = '\x80';
      }
      writev_strs(fd, {left_margin_spaces, frame_color,
                       "\N{BOX DRAWINGS LIGHT ARC DOWN AND RIGHT}", horiz_line,
                       "\N{BOX DRAWINGS LIGHT ARC DOWN AND LEFT}\e[0m",
                       clear_eol, "\n"});
    }

    // Draw content area with just borders
    for (size_t i = 0; i < all_lines.size(); ++i)
      writev_strs(fd, {left_margin_spaces, frame_color,
                       "\N{BOX DRAWINGS LIGHT VERTICAL}", content_spaces,
                       "\N{BOX DRAWINGS LIGHT VERTICAL}\e[0m", clear_eol, "\n"});

    // Draw bottom frame
    std::string horiz_line(content_width * 3, '\0');
    for (unsigned i = 0; i < content_width; ++i) {
      horiz_line[i * 3] = '\xe2';
      horiz_line[i * 3 + 1] = '\x94';
      horiz_line[i * 3 + 2] = '\x80';
    }
    writev_strs(fd,
                {left_margin_spaces, frame_color,
                 "\N{BOX DRAWINGS LIGHT ARC UP AND RIGHT}", horiz_line,
                 "\N{BOX DRAWINGS LIGHT ARC UP AND LEFT}\e[0m", clear_eol,
                 "\n"});
  } else if (frame == frame_type::background) {
    // Draw top frame
    if (!title.empty()) {
      auto metrics = measure_text(title, content_width);
      unsigned remaining = content_width - metrics.display_width;
      std::string lower_half(remaining * 3, '\0');
      for (unsigned i = 0; i < remaining; ++i) {
        lower_half[i * 3] = '\xe2';
        lower_half[i * 3 + 1] = '\x96';
        lower_half[i * 3 + 2] = '\x84';
      }
      writev_strs(fd, {left_margin_spaces, frame_color,
                       "\N{QUADRANT LOWER RIGHT}",
                       color_escape(title_fg, true), color_escape(text_bg, false),
                       title.substr(0, metrics.byte_length), "\e[0m",
                       frame_color, lower_half,
                       "\N{QUADRANT LOWER LEFT}\e[0m", clear_eol, "\n"});
    } else {
      std::string lower_half(content_width * 3, '\0');
      for (unsigned i = 0; i < content_width; ++i) {
        lower_half[i * 3] = '\xe2';
        lower_half[i * 3 + 1] = '\x96';
        lower_half[i * 3 + 2] = '\x84';
      }
      writev_strs(fd, {left_margin_spaces, frame_color,
                       "\N{QUADRANT LOWER RIGHT}", lower_half,
                       "\N{QUADRANT LOWER LEFT}\e[0m", clear_eol, "\n"});
    }

    // Draw content area with just borders
    for (size_t i = 0; i < all_lines.size(); ++i)
      writev_strs(fd,
                  {left_margin_spaces, frame_color, "\N{RIGHT HALF BLOCK}",
                   bg_color, content_spaces, "\e[0m", frame_color,
                   "\N{LEFT HALF BLOCK}\e[0m", clear_eol, "\n"});

    // Draw bottom frame
    std::string upper_half(content_width * 3, '\0');
    for (unsigned i = 0; i < content_width; ++i) {
      upper_half[i * 3] = '\xe2';
      upper_half[i * 3 + 1] = '\x96';
      upper_half[i * 3 + 2] = '\x80';
    }
    writev_strs(fd,
                {left_margin_spaces, frame_color, "\N{QUADRANT UPPER RIGHT}",
                 upper_half, "\N{QUADRANT UPPER LEFT}\e[0m", clear_eol, "\n"});
  } else {
    // No frame - just draw empty lines with background
    for (size_t i = 0; i < all_lines.size(); ++i)
      writev_strs(fd, {left_margin_spaces, bg_color, content_spaces, "\e[0m",
                       clear_eol, "\n"});
  }

  // Step 2: Move cursor back to fill in content
  if (!all_lines.empty()) {
    // Move back to first content line
    unsigned lines_to_move = all_lines.size();
    if (frame != frame_type::none)
      lines_to_move++; // Skip bottom frame
    move_cursor_up(lines_to_move);

    // Fill in each line of content
    for (const auto &line : all_lines) {
      // Position cursor after left margin and left border
      unsigned column = left_margin;
      if (frame != frame_type::none)
        column++; // Skip left border
      std::string move_right =
          column > 0 ? std::format("\e[{}C", column) : "";

      writev_strs(fd, {"\r", move_right, text_color, bg_color, line, "\e[0m\n"});
    }

    // Move cursor past bottom frame to line after widget
    if (frame != frame_type::none) {
      write_str(fd, "\n");
    }
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
          break_point = last_space; // Don't include the space in the line
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
