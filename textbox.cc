#include "textbox.hh"
#include "srchilite_map.hh"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <format>
#include <iterator>
#include <sstream>
#include <string_view>

#include <unistd.h>
#include <sys/uio.h>

#include <unistr.h>
#include <uniwidth.h>

#include <srchilite/langmap.h>
#include <srchilite/sourcehighlight.h>

namespace widget {

  namespace {

    /// Helper to write string to terminal
    inline void write_str(int fd, std::string_view str)
    {
      ::write(fd, str.data(), str.size());
    }

    /// Helper to write multiple strings to terminal using writev
    inline void writev_strs(int fd, std::initializer_list<std::string_view> strs)
    {
      std::vector<iovec> iov;
      iov.resize(strs.size());
      size_t count = 0;

      for (const auto& str : strs)
        if (! str.empty()) {
          iov[count].iov_base = const_cast<char*>(str.data());
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
    text_metrics measure_text(const std::string& text, unsigned max_width = UINT_MAX)
    {
      text_metrics result;
      const char* p = text.data();
      const char* end = p + text.size();

      while (p < end && result.display_width < max_width) {
        // Check for escape sequences
        if (*p == '\e') {
          const char* seq_start = p;
          ++p;
          if (p < end && *p == '[') {
            // CSI sequence
            ++p;
            while (p < end && (*p == ';' || (*p >= '0' && *p <= '9') || (*p >= '<' && *p <= '?')))
              ++p;
            if (p < end && (*p >= '@' && *p <= '~'))
              ++p;
          } else if (p < end && *p == ']') {
            // OSC sequence
            ++p;
            while (p < end && *p != '\a' && ! (*p == '\e' && p + 1 < end && *(p + 1) == '\\'))
              ++p;
            if (p < end && *p == '\a')
              ++p;
            else if (p + 1 < end && *p == '\e' && *(p + 1) == '\\')
              p += 2;
          } else if (p < end && (*p == '(' || *p == ')' || *p == '*' || *p == '+')) {
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
        const uint8_t* u8p = reinterpret_cast<const uint8_t*>(p);
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

    /// Replace \e[K and \e[0K with spaces and cursor repositioning
    /// @param text Text containing potential clear-to-EOL sequences
    /// @param content_width Width to clear to
    /// @return Text with escape sequences replaced
    std::string replace_clear_eol(const std::string& text, unsigned content_width)
    {
      std::string result;
      size_t pos = 0;

      while (pos < text.size()) {
        // Look for \e[K or \e[0K
        size_t esc_pos = text.find("\e[", pos);
        if (esc_pos == std::string::npos) {
          // No more escape sequences, append rest
          result += text.substr(pos);
          break;
        }

        // Check if it's a clear-to-EOL sequence
        size_t check_pos = esc_pos + 2;
        bool is_clear_eol = false;
        unsigned num_chars = 0;

        if (check_pos < text.size() && text[check_pos] == 'K') {
          // \e[K
          is_clear_eol = true;
          num_chars = 3;
        } else if (check_pos + 1 < text.size() && text[check_pos] == '0' && text[check_pos + 1] == 'K') {
          // \e[0K
          is_clear_eol = true;
          num_chars = 4;
        }

        if (is_clear_eol) {
          // Append text up to escape sequence
          result += text.substr(pos, esc_pos - pos);

          // Calculate current display width
          unsigned current_width = measure_text(result, content_width).display_width;

          // Add spaces to fill to content_width
          if (current_width < content_width) {
            unsigned spaces_needed = content_width - current_width;
            result += std::string(spaces_needed, ' ');
            // Move cursor back
            result += std::format("\e[{}D", spaces_needed);
          }

          pos = esc_pos + num_chars;
        } else {
          // Not a clear-to-EOL, just copy the escape sequence start and continue
          result += text.substr(pos, check_pos - pos);
          pos = check_pos;
        }
      }

      return result;
    }

  } // anonymous namespace

  textbox::textbox(const terminal::info& term, const std::string& name)
      : term_info{term}, widget_name{name}, title{name}
  {
    // Move to column 1 to ensure clean starting position
    write_str(term_info.get_fd(), "\r");

    // Initialize with one empty paragraph
    paragraphs.emplace_back();

    // Initialize title foreground from terminal default
    title_fg = term_info.default_foreground;
    code_lang_fg = term_info.default_foreground;
  }

  textbox::~textbox()
  {
    // Cursor is already positioned at the line after widget (from last newline)
    // No action needed
  }

  void textbox::set_title(const std::string& new_title)
  {
    title = new_title;
    if (has_been_drawn)
      render();
  }

  void textbox::add_text(const std::string& text)
  {
    for (char ch : text) {
      if (ch == '\n') {
        // Close current paragraph if not empty, start new one
        if (! paragraphs.back().content.empty()) {
          assert(paragraphs.back().is_reflow);
          paragraphs.emplace_back();
        }
      } else if (ch != '\r')
        // Append character to current paragraph
        paragraphs.back().content += ch;
    }

    render();
  }

  void textbox::add_block(const std::vector<std::string>& lines)
  {
    std::string block_content;
    for (const auto& line : lines) {
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

  void textbox::add_markdown(const std::string& markdown)
  {
    raw_markdown += markdown;
    parse_markdown();
    render();
  }

  void textbox::set_frame(frame_type ft)
  {
    frame = ft;
    if (has_been_drawn)
      render();
  }

  void textbox::set_left_margin(unsigned margin)
  {
    left_margin = margin;
    if (has_been_drawn)
      render();
  }

  void textbox::set_right_margin(unsigned margin)
  {
    right_margin = margin;
    if (has_been_drawn)
      render();
  }

  void textbox::set_text_foreground(uint8_t r, uint8_t g, uint8_t b)
  {
    text_fg = {r, g, b};
    if (has_been_drawn)
      render();
  }

  void textbox::set_text_background(uint8_t r, uint8_t g, uint8_t b)
  {
    text_bg = {r, g, b};
    if (has_been_drawn)
      render();
  }

  void textbox::set_frame_foreground(uint8_t r, uint8_t g, uint8_t b)
  {
    frame_fg = {r, g, b};
    if (has_been_drawn)
      render();
  }

  void textbox::draw()
  {
    render();
  }

  void textbox::parse_markdown()
  {
    // Clear all paragraphs
    paragraphs.clear();
    paragraphs.emplace_back(); // Start with empty paragraph

    if (raw_markdown.empty())
      return;

    // Reset heading counters
    std::ranges::fill(heading_counters, 0);

    // Track headings for dynamic renumbering
    size_t min_heading_level = max_heading_level + 1;
    struct heading_entry {
      size_t para_idx;
      size_t level;
      std::string text;
    };
    std::vector<heading_entry> heading_list;

    // List state tracking
    struct list_level {
      bool is_ordered = false;
      unsigned counter = 0;
      unsigned indent = 0; // Actual indentation in spaces
    };
    std::vector<list_level> list_stack;
    std::vector<list_level> blockquote_list_stack;

    // Helper function to check and process inline formatting at current position
    // Returns: {formatted_text, new_position} or {"", 0} if no formatting found
    auto try_inline_format = [&](const std::string& text, size_t pos_in) -> std::pair<std::string, size_t> {
      if (pos_in >= text.size())
        return {"", 0};

      char ch = text[pos_in];

      // Check for inline code `...`
      if (ch == '`') {
        size_t end = text.find('`', pos_in + 1);
        if (end != std::string::npos) {
          std::string code_text = text.substr(pos_in + 1, end - pos_in - 1);
          return {color_escape(code_fg, true) + color_escape(code_bg, false) + code_text + "\e[0m", end + 1};
        }
      }

      // Check for bold **...**
      if (pos_in + 1 < text.size() && ch == '*' && text[pos_in + 1] == '*') {
        size_t end = text.find("**", pos_in + 2);
        // Deliberately use end even if it is npos.
        std::string bold_text = text.substr(pos_in + 2, end - pos_in - 2);
        return {"\e[1m" + color_escape(bold_fg, true) + bold_text + "\e[0m", end + 2};
      }

      // Check for italic *...*
      if (ch == '*') {
        size_t end = text.find('*', pos_in + 1);
        // Deliberately use end even if it is npos.
        std::string italic_text = text.substr(pos_in + 1, end - pos_in - 1);
        return {"\e[3m" + color_escape(italic_fg, true) + italic_text + "\e[0m", end + 1};
      }

      // Check for italic _..._
      if (ch == '_') {
        size_t end = text.find('_', pos_in + 1);
        // Deliberately use end even if it is npos.
        std::string italic_text = text.substr(pos_in + 1, end - pos_in - 1);
        return {"\e[3m" + color_escape(italic_fg, true) + italic_text + "\e[0m", end + 1};
      }

      // Check for strikethrough ~~...~~
      if (pos_in + 1 < text.size() && ch == '~' && text[pos_in + 1] == '~') {
        size_t end = text.find("~~", pos_in + 2);
        // Deliberately use end even if it is npos.
        std::string strike_text = text.substr(pos_in + 2, end - pos_in - 2);
        return {"\e[9m" + color_escape(strikethrough_fg, true) + strike_text + "\e[0m", end + 2};
      }

      return {"", 0};
    };

    // Helper function to process inline formatting (bold, italic, code, strikethrough)
    auto process_inline_formatting = [&](const std::string& text) -> std::string {
      std::string result;
      size_t fmt_pos = 0;

      while (fmt_pos < text.size()) {
        auto [formatted, new_pos] = try_inline_format(text, fmt_pos);
        if (new_pos > 0) {
          result += formatted;
          fmt_pos = new_pos;
        } else {
          result += text[fmt_pos];
          ++fmt_pos;
        }
      }

      return result;
    };

    size_t pos = 0;
    std::string current_para;
    bool at_line_start = true;

    while (pos < raw_markdown.size()) {
      // Check for heading at start of line
      if (at_line_start && raw_markdown[pos] == '#') {
        // Count heading level
        size_t heading_level = 0;
        size_t hash_pos = pos;
        while (hash_pos < raw_markdown.size() && raw_markdown[hash_pos] == '#' && heading_level < max_heading_level) {
          ++heading_level;
          ++hash_pos;
        }

        // Check for space after hashes
        if (hash_pos < raw_markdown.size() && (raw_markdown[hash_pos] == ' ' || raw_markdown[hash_pos] == '\t')) {
          assert(heading_level > 0);
          assert(heading_level <= max_heading_level);

          // Extract heading text until newline
          ++hash_pos; // Skip space
          size_t heading_start = hash_pos;
          size_t heading_end = raw_markdown.find('\n', heading_start);
          if (heading_end == std::string::npos)
            heading_end = raw_markdown.size();

          std::string heading_text = raw_markdown.substr(heading_start, heading_end - heading_start);

          // Trim trailing whitespace
          size_t last = heading_text.find_last_not_of(" \t\r");
          if (last != std::string::npos)
            heading_text = heading_text.substr(0, last + 1);

          // Skip leading manual numbering (e.g., "1. " or "1.2.3 ")
          size_t text_start = 0;
          // Skip leading whitespace
          while (text_start < heading_text.size() && (heading_text[text_start] == ' ' || heading_text[text_start] == '\t'))
            ++text_start;
          // Skip digits and dots
          while (text_start < heading_text.size() && (::isdigit(heading_text[text_start]) || heading_text[text_start] == '.'))
            ++text_start;
          // Skip whitespace after the number
          while (text_start < heading_text.size() && (heading_text[text_start] == ' ' || heading_text[text_start] == '\t'))
            ++text_start;
          // Extract the actual heading text
          if (text_start > 0 && text_start < heading_text.size())
            heading_text = heading_text.substr(text_start);

          // Update minimum heading level if this is shallower
          if (heading_level < min_heading_level)
            min_heading_level = heading_level;

          // Save any pending paragraph
          if (! current_para.empty()) {
            paragraphs.back().content = current_para;
            current_para.clear();
            paragraphs.emplace_back();
          }

          // Save any pending paragraph content first
          if (! current_para.empty()) {
            if (! paragraphs.back().content.empty())
              paragraphs.emplace_back();
            paragraphs.back().content = current_para;
            current_para.clear();
            paragraphs.emplace_back(); // Create new empty paragraph for heading
          }

          // Add heading as new paragraph (always fixed to preserve formatting)
          if (! paragraphs.back().content.empty())
            paragraphs.emplace_back();

          // Record this heading
          size_t para_idx = paragraphs.size() - 1;
          heading_list.push_back({para_idx, heading_level, heading_text});

          // Set placeholder content
          paragraphs.back().content = "";
          paragraphs.back().is_reflow = false;

          // Renumber all headings, tracking minimum level as we go
          std::ranges::fill(heading_counters, 0);

          unsigned last_displayed_counter = 0;
          size_t min_level_so_far = max_heading_level + 1;

          for (const auto& h : heading_list) {
            // Update minimum level encountered so far
            if (h.level < min_level_so_far)
              min_level_so_far = h.level;

            // Calculate normalized level based on minimum seen so far
            size_t norm_level = h.level - min_level_so_far;

            // Check if all parent counters are zero
            bool all_parents_zero = true;
            for (size_t i = 0; i < norm_level; ++i) {
              if (heading_counters[i] != 0) {
                all_parents_zero = false;
                break;
              }
            }

            // If all parents are zero, we're at top display level - use sequential counter
            if (all_parents_zero) {
              ++last_displayed_counter;
              heading_counters[norm_level] = last_displayed_counter;
            } else {
              // Normal hierarchical numbering
              ++heading_counters[norm_level];
            }

            // Reset deeper levels
            std::fill_n(heading_counters.data() + norm_level + 1, max_heading_level - norm_level - 1, 0);

            // Build numbering, skipping the minimum level counter
            std::string numbering;
            if (norm_level == 0) {
              // Minimum level heading: just pilcrow with no number
              numbering = "\N{PILCROW SIGN}  ";
            } else {
              // Sub-headings: skip leading zeros and the minimum level counter (start from level 1)
              size_t display_start = 1;
              while (display_start < norm_level && heading_counters[display_start] == 0)
                ++display_start;

              numbering = std::format("\N{PILCROW SIGN} {}", heading_counters[display_start]);
              for (size_t i = display_start + 1; i <= norm_level; ++i)
                std::format_to(std::back_inserter(numbering), ".{}", heading_counters[i]);
              numbering += "  ";
            }

            paragraphs[h.para_idx].content = color_escape(hx_fg[h.level - 1], true) + numbering + h.text + "\e[0m";
          }

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
      if (at_line_start && pos + 3 <= raw_markdown.size() && raw_markdown.substr(pos, 3) == "```") {
        // Save any pending paragraph content
        if (! current_para.empty()) {
          if (! paragraphs.back().content.empty())
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
        bool has_highlighting = false;
        std::stringstream code_stream;
        if (auto srchilite = find_source_highlight_data(language)) {
          // Create source-highlight instance
          srchilite::SourceHighlight highlighter{"esc256.outlang"};

          // Use input streams as needed by the source-highlight API.
          std::istringstream input{code};

          // Highlight the code
          highlighter.highlight(input, code_stream, srchilite->fname);
          if (! code_stream.eof()) {
            code_stream.seekg(0);
            language = srchilite->stdname;
            has_highlighting = true;
          }
        }

        if (! has_highlighting)
          code_stream = std::stringstream{code};

        // Build the complete code block with indentation and optional header
        std::string code_block;

        // Add language header if language is specified
        if (! language.empty())
          code_block += "    " + color_escape(code_lang_fg, true) + "🭪" + language + "🭨" + color_escape(code_lang_fg, false) + "\n";

        std::string fg_escape = color_escape(code_fg, true);
        std::string bg_escape = color_escape(code_block_bg, false);

        // Split code into lines and indent each line by 4 spaces
        std::string line;
        while (std::getline(code_stream, line)) {
          if (has_highlighting) {
            // Replace reset sequences to preserve background.  Replace \e[0m and \e[m with \e[39m
            // (reset foreground only) + background
            std::string replacement = fg_escape + bg_escape;
            size_t pos_replace = 0;
            while ((pos_replace = line.find("\e[0m", pos_replace)) != std::string::npos) {
              line.replace(pos_replace, 4, replacement);
              pos_replace += replacement.length();
            }
            pos_replace = 0;
            while ((pos_replace = line.find("\e[m", pos_replace)) != std::string::npos) {
              line.replace(pos_replace, 3, replacement);
              pos_replace += replacement.length();
            }
            pos_replace = 0;
            while ((pos_replace = line.find("\e[00;38;", pos_replace)) != std::string::npos) {
              line.erase(pos_replace + 2, 3);
              pos_replace += 5;
            }
          }

          // Apply background color and 4-space indentation
          code_block += "    " + fg_escape + bg_escape;

          code_block += line + "\n";
        }

        // Add as fixed paragraph
        if (! paragraphs.back().content.empty())
          paragraphs.emplace_back();
        paragraphs.back().content = code_block;
        paragraphs.back().is_reflow = false;

        // Skip past closing ```
        pos = code_end;
        if (pos < raw_markdown.size() && raw_markdown[pos] == '\n')
          ++pos;
        if (pos + 3 <= raw_markdown.size() && raw_markdown.substr(pos, 3) == "```")
          pos += 3;
        if (pos < raw_markdown.size() && raw_markdown[pos] == '\n') {
          ++pos;
          at_line_start = true;
        }

        continue;
      }

      // Check for blockquotes (only at line start)
      if (at_line_start && raw_markdown[pos] == '>') {
        // Count blockquote level, allowing optional space between > markers
        size_t quote_pos = pos;
        unsigned quote_level = 0;
        while (quote_pos < raw_markdown.size()) {
          if (raw_markdown[quote_pos] == '>') {
            ++quote_level;
            ++quote_pos;
            // Skip optional space after >
            if (quote_pos < raw_markdown.size() && raw_markdown[quote_pos] == ' ')
              ++quote_pos;
          } else {
            break;
          }
        }

        // Extract blockquote content until newline
        size_t quote_end = raw_markdown.find('\n', quote_pos);
        if (quote_end == std::string::npos)
          quote_end = raw_markdown.size();

        std::string quote_content_raw = raw_markdown.substr(quote_pos, quote_end - quote_pos);

        // Check if blockquote content is a list item
        size_t content_pos = 0;
        unsigned list_level = 0;
        bool is_list = false;
        bool is_ordered = false;

        // Count leading spaces for list nesting
        unsigned leading_spaces = 0;
        while (content_pos < quote_content_raw.size() && quote_content_raw[content_pos] == ' ') {
          ++leading_spaces;
          ++content_pos;
        }

        // Check for list markers
        if (content_pos + 1 < quote_content_raw.size() && (quote_content_raw[content_pos] == '-' || quote_content_raw[content_pos] == '*' || quote_content_raw[content_pos] == '+') && quote_content_raw[content_pos + 1] == ' ') {
          is_list = true;
          is_ordered = false;
          content_pos += 2;
        } else if (content_pos < quote_content_raw.size() && quote_content_raw[content_pos] >= '0' && quote_content_raw[content_pos] <= '9') {
          size_t digit_end = content_pos;
          while (digit_end < quote_content_raw.size() && quote_content_raw[digit_end] >= '0' && quote_content_raw[digit_end] <= '9')
            ++digit_end;

          if (digit_end + 1 < quote_content_raw.size() && quote_content_raw[digit_end] == '.' && quote_content_raw[digit_end + 1] == ' ') {
            is_list = true;
            is_ordered = true;
            content_pos = digit_end + 2;
          }
        }

        // Apply relative indentation logic if this is a list item
        if (is_list) {
          // Pop levels that have greater indentation than current
          while (! blockquote_list_stack.empty() && blockquote_list_stack.back().indent > leading_spaces)
            blockquote_list_stack.pop_back();

          // Add new level if this indentation is greater than current top level
          if (blockquote_list_stack.empty() || blockquote_list_stack.back().indent < leading_spaces)
            blockquote_list_stack.push_back({is_ordered, 0, leading_spaces});

          list_level = blockquote_list_stack.size() - 1;
        } else
          // Not a list item - clear blockquote list stack
          blockquote_list_stack.clear();

        // Extract actual content (after list marker if present)
        std::string quote_content = quote_content_raw.substr(content_pos);

        // Process inline formatting
        std::string formatted_content = process_inline_formatting(quote_content);

        // Save any pending paragraph
        if (! current_para.empty()) {
          if (! paragraphs.back().content.empty())
            paragraphs.emplace_back();
          paragraphs.back().content = current_para;
          current_para.clear();
          paragraphs.emplace_back();
        }

        // Add as blockquote paragraph with metadata
        if (! paragraphs.back().content.empty())
          paragraphs.emplace_back();
        paragraphs.back().content = formatted_content;
        paragraphs.back().is_reflow = true; // Allow wrapping for all blockquote content
        paragraphs.back().is_blockquote = true;
        paragraphs.back().blockquote_level = quote_level;
        paragraphs.back().is_list_item = is_list;
        paragraphs.back().is_ordered = is_ordered;
        paragraphs.back().list_level = list_level;

        // Move past blockquote line
        pos = quote_end;
        if (pos < raw_markdown.size() && raw_markdown[pos] == '\n') {
          ++pos;
          at_line_start = true;
        }

        continue;
      }

      // Check for list items (only at line start)
      if (at_line_start) {
        // Clear blockquote list stack since we're not in a blockquote
        blockquote_list_stack.clear();

        // Count leading spaces
        size_t indent_pos = pos;
        unsigned leading_spaces = 0;
        while (indent_pos < raw_markdown.size() && raw_markdown[indent_pos] == ' ') {
          ++leading_spaces;
          ++indent_pos;
        }

        bool is_list_item = false;
        bool is_ordered = false;
        size_t content_start = indent_pos;

        // Check for unordered list markers (-, *, +)
        if (indent_pos + 1 < raw_markdown.size() && (raw_markdown[indent_pos] == '-' || raw_markdown[indent_pos] == '*' || raw_markdown[indent_pos] == '+') && raw_markdown[indent_pos + 1] == ' ') {
          is_list_item = true;
          is_ordered = false;
          content_start = indent_pos + 2;
        }
        // Check for ordered list markers (digit(s) followed by .)
        else if (indent_pos < raw_markdown.size() && raw_markdown[indent_pos] >= '0' && raw_markdown[indent_pos] <= '9') {
          size_t digit_end = indent_pos;
          while (digit_end < raw_markdown.size() && raw_markdown[digit_end] >= '0' && raw_markdown[digit_end] <= '9')
            ++digit_end;

          if (digit_end + 1 < raw_markdown.size() && raw_markdown[digit_end] == '.' && raw_markdown[digit_end + 1] == ' ') {
            is_list_item = true;
            is_ordered = true;
            content_start = digit_end + 2;
          }
        }

        if (is_list_item) {
          // Extract list item content until newline
          size_t item_end = raw_markdown.find('\n', content_start);
          if (item_end == std::string::npos)
            item_end = raw_markdown.size();

          std::string item_content_raw = raw_markdown.substr(content_start, item_end - content_start);

          // Process inline formatting in list item content
          std::string item_content = process_inline_formatting(item_content_raw);

          // Save any pending paragraph
          if (! current_para.empty()) {
            if (! paragraphs.back().content.empty())
              paragraphs.emplace_back();
            paragraphs.back().content = current_para;
            current_para.clear();
            paragraphs.emplace_back();
          }

          // Adjust list stack based on relative indentation
          // Pop levels that have greater indentation than current
          while (! list_stack.empty() && list_stack.back().indent > leading_spaces)
            list_stack.pop_back();

          // Add new level if this indentation is greater than current top level
          if (list_stack.empty() || list_stack.back().indent < leading_spaces)
            list_stack.push_back({is_ordered, 0, leading_spaces});

          unsigned current_level = list_stack.size() - 1;

          // Add as list item paragraph with metadata
          if (! paragraphs.back().content.empty())
            paragraphs.emplace_back();
          paragraphs.back().content = item_content;
          paragraphs.back().is_reflow = true; // Allow wrapping
          paragraphs.back().is_list_item = true;
          paragraphs.back().is_ordered = is_ordered;
          paragraphs.back().list_level = current_level;

          // Move past list item
          pos = item_end;
          if (pos < raw_markdown.size() && raw_markdown[pos] == '\n') {
            ++pos;
            at_line_start = true;
          }

          continue;
        }

        // If we had a list but this isn't a list item, clear the list stack
        if (! is_list_item && ! list_stack.empty()) {
          // Check if this is a blank line (which ends the list)
          if (indent_pos < raw_markdown.size() && raw_markdown[indent_pos] == '\n') {
            list_stack.clear();
          }
        }
      }

      // Non-heading, non-code-block, non-list character - no longer at line start
      at_line_start = false;

      // Process regular text with inline formatting
      char ch = raw_markdown[pos];

      if (ch == '\n') {
        ++pos;
        at_line_start = true;
        // Check for paragraph break (double newline)
        if (pos < raw_markdown.size() && raw_markdown[pos] == '\n') {
          // Paragraph break - ends list context
          list_stack.clear();
          blockquote_list_stack.clear();
          if (! current_para.empty()) {
            if (! paragraphs.back().content.empty())
              paragraphs.emplace_back();
            paragraphs.back().content = current_para;
            current_para.clear();
            paragraphs.emplace_back(); // Create new empty paragraph
          }
          ++pos; // Skip second newline
        } else if (! current_para.empty()) {
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

      // Try to process inline formatting
      auto [formatted, new_pos] = try_inline_format(raw_markdown, pos);
      if (new_pos > 0) {
        current_para += formatted;
        pos = new_pos;
      } else {
        // Regular character
        current_para += ch;
        ++pos;
      }
    }

    // Add final paragraph if not empty
    if (! current_para.empty()) {
      if (! paragraphs.back().content.empty())
        paragraphs.emplace_back();
      paragraphs.back().content = current_para;
    }

    // Ensure there's always an empty paragraph at the end
    if (paragraphs.empty() || ! paragraphs.back().content.empty())
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

  void textbox::render()
  {
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

    // Use different bullets for different levels
    static constexpr const std::array bullets{
      "\N{BLACK CIRCLE}", // ● level 0
      "\N{WHITE CIRCLE}", // ○ level 1
      "-",                // - level 2
      "*",                // * level 3
      "\N{MIDDLE DOT}",   // · level 4+
    };

    // Clear to end of line when re-rendering to remove artifacts
    std::string clear_eol = has_been_drawn && right_margin > 0 ? "\e[K" : "";

    // Create left margin string
    std::string left_margin_spaces(left_margin, ' ');

    // Calculate total content lines needed FIRST (before moving cursor)
    std::vector<std::string> all_lines;

    // Track ordered list counters at each level for rendering
    std::vector<unsigned> list_counters;

    for (size_t i = 0; i < paragraphs.size(); ++i) {
      const auto& para = paragraphs[i];

      if (para.content.empty())
        continue;

      if (para.is_blockquote || para.is_list_item || para.is_reflow) {
        // Blockquote - wrap with indentation and quote markers
        unsigned prefix_width = para.blockquote_level * 2;
        std::string blockquote_prefix;

        // Build blockquote prefix with ▌ markers at start of each level
        for (unsigned j = 0; j < para.blockquote_level; ++j)
          blockquote_prefix += "\N{LEFT HALF BLOCK} "; // ▌ + 1 space

        // Build list item prefix (indentation + bullet/number)
        std::string list_prefix;

        // Check if this blockquote is also a list item
        if (para.is_list_item) {
          // Adjust counter array size to match list level
          if (list_counters.size() <= para.list_level)
            list_counters.resize(para.list_level + 1, 0);

          // Increment counter for ordered lists at this level
          if (para.is_ordered)
            ++list_counters[para.list_level];
          else
            // Reset for the next time this level is used for enumeration instead of itemization.
            // Not relevant for displaying itemization but still needed.
            list_counters[para.list_level] = 0;

          // Reset deeper level counters
          std::fill(list_counters.begin() + para.list_level + 1, list_counters.end(), 0);

          // Add bullet or number
          if (para.is_ordered)
            list_prefix = std::format("{}{:{}s}{}. ", blockquote_prefix, "", para.list_level * 2, list_counters[para.list_level]);
          else {
            auto bullet_index = std::min(para.list_level, static_cast<unsigned>(bullets.size() - 1));
            list_prefix = std::format("{}{:{}s}{} ", blockquote_prefix, "", para.list_level * 2, bullets[bullet_index]);
          }

          prefix_width = calculate_display_width(list_prefix);
        } else
          // Not a list item - use blockquote prefix alone
          list_prefix = blockquote_prefix;

        // Calculate available width for content
        unsigned available_width = content_width > prefix_width ? content_width - prefix_width : 1;

        // Wrap content
        auto wrapped_lines = wrap_paragraph(para.content, available_width);

        // Add first line with full prefix
        if (! wrapped_lines.empty()) {
          all_lines.push_back(std::move(list_prefix) + wrapped_lines[0]);

          // Add continuation lines with hanging indent
          for (size_t j = 1; j < wrapped_lines.size(); ++j)
            all_lines.push_back(std::format("{:{}}{}", blockquote_prefix, prefix_width, wrapped_lines[j]));
        }
      } else {
        // Fixed paragraph - split into lines
        assert(para.blockquote_level == 0);
        assert(para.list_level == 0);

        std::string::size_type pos = 0;
        while (pos < para.content.size()) {
          auto newline_pos = para.content.find('\n', pos);
          if (newline_pos == std::string::npos)
            newline_pos = para.content.size();
          std::string line = para.content.substr(pos, newline_pos - pos);
          all_lines.push_back(truncate_text(std::move(line), content_width));
          pos = newline_pos + 1;
        }
      }

      // Empty line between paragraphs
      if (i + 1 < paragraphs.size() && ! paragraphs[i + 1].content.empty()) {
        bool add_empty_line = true;

        // Skip empty line for consecutive blockquotes at any level (same, deeper, or shallower)
        if (para.is_blockquote && paragraphs[i + 1].is_blockquote)
          add_empty_line = false;
        // Skip empty line for consecutive list items of the same type at any level
        else if (para.is_list_item && paragraphs[i + 1].is_list_item && para.is_ordered == paragraphs[i + 1].is_ordered)
          add_empty_line = false;
        // Skip if next item is at a different nesting level (deeper or shallower)
        else if (para.is_list_item && paragraphs[i + 1].is_list_item && paragraphs[i + 1].list_level != para.list_level)
          add_empty_line = false;
        // Add empty line only if list types differ at the same level
        else if (para.is_list_item && paragraphs[i + 1].is_list_item && para.list_level == paragraphs[i + 1].list_level && para.is_ordered != paragraphs[i + 1].is_ordered)
          add_empty_line = true;

        if (add_empty_line)
          all_lines.push_back(std::string(content_width, ' '));
      }
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
    std::string bg_color = (frame == frame_type::background) ? color_escape(text_bg, false) : "";
    std::string content_spaces(content_width, ' ');

    if (frame == frame_type::none) [[unlikely]] {
      // No frame - just draw empty lines with background
      std::string tmpstr;
      tmpstr.reserve(4 * content_width);
      tmpstr = left_margin_spaces;
      tmpstr.append(bg_color);
      tmpstr.append(content_spaces);
      tmpstr.append("\e[0m");
      tmpstr.append(clear_eol);
      tmpstr.append("\n");
      for (size_t i = 0; i < all_lines.size(); ++i)
        write_str(fd, tmpstr);
    } else {
      bool is_line = frame == frame_type::line;

      // Draw top frame
      std::string tmpstr;
      tmpstr.reserve(4 * content_width);
      tmpstr = left_margin_spaces;
      tmpstr.append(frame_color);
      tmpstr.append(is_line ? "\N{BOX DRAWINGS LIGHT ARC DOWN AND RIGHT}" : "\N{QUADRANT LOWER RIGHT}");
      auto remaining = content_width;
      if (! title.empty()) {
        auto metrics = measure_text(title, content_width);
        remaining -= metrics.display_width;
        tmpstr.append(color_escape(title_fg, true));
        tmpstr.append(color_escape(text_bg, false));
        tmpstr.append(title.substr(0, metrics.byte_length));
        tmpstr.append("\e[0m");
      }
      tmpstr.append(frame_color);
      for (unsigned i = 0; i < remaining; ++i) {
        static const char light_horiz[] = "\N{BOX DRAWINGS LIGHT HORIZONTAL}";
        static const char lower_half[] = "\N{LOWER HALF BLOCK}";
        tmpstr.append(is_line ? light_horiz : lower_half);
      }
      tmpstr.append(is_line ? "\N{BOX DRAWINGS LIGHT ARC DOWN AND LEFT}\e[0m" : "\N{QUADRANT LOWER LEFT}\e[0m");
      tmpstr.append(clear_eol);
      tmpstr.append("\n");
      write_str(fd, tmpstr);

      // Draw content area with just borders
      tmpstr = left_margin_spaces;
      tmpstr.append(frame_color);
      tmpstr.append(is_line ? "\N{BOX DRAWINGS LIGHT VERTICAL}" : "\N{RIGHT HALF BLOCK}");
      tmpstr.append(bg_color);
      tmpstr.append(content_spaces);
      tmpstr.append("\e[0m");
      tmpstr.append(frame_color);
      tmpstr.append(is_line ? "\N{BOX DRAWINGS LIGHT VERTICAL}\e[0m" : "\N{LEFT HALF BLOCK}\e[0m");
      tmpstr.append(clear_eol);
      tmpstr.append("\n");
      for (size_t i = 0; i < all_lines.size(); ++i)
        write_str(fd, tmpstr);

      // Draw bottom frame
      tmpstr = left_margin_spaces;
      tmpstr.append(frame_color);
      tmpstr.append(is_line ? "\N{BOX DRAWINGS LIGHT ARC UP AND RIGHT}" : "\N{QUADRANT UPPER RIGHT}");
      for (unsigned i = 0; i < content_width; ++i) {
        static const char light_horiz[] = "\N{BOX DRAWINGS LIGHT HORIZONTAL}";
        static const char upper_half[] = "\N{UPPER HALF BLOCK}";
        tmpstr.append(is_line ? light_horiz : upper_half);
      }
      tmpstr.append(is_line ? "\N{BOX DRAWINGS LIGHT ARC UP AND LEFT}\e[0m" : "\N{QUADRANT UPPER LEFT}\e[0m");
      tmpstr.append(clear_eol);
      tmpstr.append("\n");
      write_str(fd, tmpstr);
    }

    // Step 2: Move cursor back to fill in content
    if (! all_lines.empty()) {
      // Move back to first content line
      unsigned lines_to_move = all_lines.size();
      if (frame != frame_type::none)
        ++lines_to_move; // Skip bottom frame
      move_cursor_up(lines_to_move);

      unsigned column = left_margin;
      if (frame != frame_type::none)
        ++column; // Skip left border
      std::string move_right = column > 0 ? std::format("\e[{}C", column) : "";

      // Fill in each line of content
      for (const auto& line : all_lines) {
        // Replace \e[K sequences with spaces and cursor repositioning
        std::string processed_line = replace_clear_eol(line, content_width);

        // Position cursor after left margin and left border and then print the content.
        writev_strs(fd, {"\r", move_right, text_color, bg_color, processed_line, "\e[0m\n"});
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

  unsigned textbox::calculate_display_width(const std::string& text) const
  {
    return measure_text(text).display_width;
  }

  std::vector<std::string> textbox::wrap_paragraph(const std::string& text, unsigned width) const
  {
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
          line.append(width - line_width, ' ');

        lines.push_back(std::move(line));
        pos = break_point;

        // Skip leading spaces on next line
        while (pos < text.size() && text[pos] == ' ')
          ++pos;
      }
    }

    return lines;
  }

  std::string textbox::truncate_text(std::string&& text, unsigned width) const
  {
    auto metrics = measure_text(text, width);

    if (metrics.display_width >= width)
      text.resize(metrics.byte_length);
    else
      // Pad to width
      text.append(width - metrics.display_width, ' ');

    return text;
  }

  std::tuple<unsigned, unsigned> textbox::get_terminal_dimensions()
  {
    return term_info.get_geometry().value_or(std::tuple{24u, 80u});
  }

  void textbox::move_cursor_up(unsigned lines) const
  {
    if (lines > 0) {
      std::string seq = std::format("\e[{}A", lines);
      write_str(term_info.get_fd(), seq);
    }
  }

  std::string textbox::color_escape(const terminal::info::color& color, bool foreground) const
  {
    return std::format("\e[{};2;{};{};{}m", foreground ? "38" : "48", color.r, color.g, color.b);
  }

} // namespace widget
