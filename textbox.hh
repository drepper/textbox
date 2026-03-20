#ifndef _TEXTBOX_HH
# define _TEXTBOX_HH 1

# include <array>
# include <cstddef>
# include <cstdint>
# include <string>
# include <vector>

# include "termdetect/termdetect.hh"

namespace widget {

  /// Text widget for terminal display with frame and formatting support
  struct textbox {
    /// Frame rendering style
    enum class frame_type {
      none,      ///< No frame
      line,      ///< Line drawing characters (╭─╮│╰─╯)
      background ///< Block characters (▗▄▖▐▌▝▀▘)
    };

    /// Constructor
    /// @param term Terminal information object
    /// @param name Widget name (also used as default title)
    textbox(const terminal::info& term, const std::string& name);

    /// Destructor - moves cursor to line after widget
    ~textbox();

    /// Close the widget and optionally clear if empty
    void close();

    /// Set widget title
    /// @param new_title New title text
    void set_title(const std::string& new_title);

    /// Set whether to clear widget on destruction if empty
    /// @param clear_if_empty If true (default), clear widget lines if no content on destruction
    void set_clear_if_empty(bool clear_if_empty);

    /// Add text content with automatic paragraph wrapping
    /// @param text Text to add (newlines create paragraph breaks)
    void add_text(const std::string& text);

    /// Add fixed-format block of text
    /// @param lines Vector of text lines
    void add_block(const std::vector<std::string>& lines);

    /// Add markdown content
    /// @param markdown Markdown text to parse and add
    void add_markdown(const std::string& markdown);

    /// Set frame style
    /// @param ft Frame type
    void set_frame(frame_type ft);

    /// Set left margin
    /// @param margin Margin width in columns
    void set_left_margin(unsigned margin);

    /// Set right margin
    /// @param margin Margin width in columns
    void set_right_margin(unsigned margin);

    /// Set text foreground color
    /// @param r Red component (0-255)
    /// @param g Green component (0-255)
    /// @param b Blue component (0-255)
    void set_text_foreground(uint8_t r, uint8_t g, uint8_t b);

    /// Set text background color
    /// @param r Red component (0-255)
    /// @param g Green component (0-255)
    /// @param b Blue component (0-255)
    void set_text_background(uint8_t r, uint8_t g, uint8_t b);

    /// Set frame foreground color
    /// @param r Red component (0-255)
    /// @param g Green component (0-255)
    /// @param b Blue component (0-255)
    void set_frame_foreground(uint8_t r, uint8_t g, uint8_t b);

    /// Force widget to render immediately
    void draw();

    /// Set minimum number of lines that must remain available on screen
    /// @param lines Minimum lines to keep available (default 5)
    void set_min_lines_remaining(unsigned lines);

  private:
    /// Paragraph data structure
    struct paragraph {
      std::string content{};
      bool is_reflow = true;                              ///< True for reflowable, false for fixed
      bool is_list_item = false;                          ///< True if this is a list item
      bool is_ordered = false;                            ///< True for ordered list, false for unordered
      unsigned list_level = 0;                            ///< Nesting level (0 = top level)
      bool is_blockquote = false;                         ///< True if this is a blockquote
      unsigned blockquote_level = 0;                      ///< Blockquote nesting level (0 = not a blockquote)
      bool is_table = false;                              ///< True if this is a table
      std::vector<std::vector<std::string>> table_data{}; ///< Table cells (rows x cols)
      std::vector<char> table_alignment{};                ///< Column alignment ('l', 'c', 'r')
    };

    const terminal::info& term_info;
    std::string widget_name;
    std::string title;

    frame_type frame = frame_type::line;
    unsigned left_margin = 0;
    unsigned right_margin = 0;

    terminal::info::color text_fg{127, 127, 127};
    terminal::info::color text_bg{57, 17, 17};
    terminal::info::color frame_fg{129, 27, 27};
    terminal::info::color title_fg{};

    // Markdown highlighting colors
    terminal::info::color bold_fg{255, 255, 255};
    terminal::info::color italic_fg{180, 180, 255};
    terminal::info::color strikethrough_fg{100, 100, 100};
    terminal::info::color code_fg{255, 200, 100};
    terminal::info::color code_bg{40, 40, 40};
    terminal::info::color code_block_bg{40, 40, 40};
    terminal::info::color code_lang_fg{};

    // Heading colors (decreasing brightness)
    static constexpr size_t max_heading_level = 6;
    std::array<terminal::info::color, max_heading_level> hx_fg{{{255, 255, 255}, {230, 230, 230}, {200, 200, 200}, {170, 170, 170}, {140, 140, 140}, {110, 110, 110}}};

    std::vector<paragraph> paragraphs{};
    std::string raw_markdown{};

    // Heading counters for hierarchical numbering
    std::array<unsigned, max_heading_level> heading_counters = {0, 0, 0, 0, 0, 0};

    // Track widget height for re-rendering
    unsigned widget_height = 0;

    // Track whether widget has been drawn
    bool has_been_drawn = false;

    // Whether to clear widget on destruction if empty (default true)
    bool clear_if_empty = true;

    // Track whether widget has been closed
    bool is_closed = false;

    // Minimum number of lines to keep available on screen (default 5)
    unsigned min_lines_remaining = 5;

    /// Render the widget to the terminal
    void render();

    /// Parse markdown content and populate paragraphs
    void parse_markdown();

    /// Calculate display width of text (accounting for multi-column chars and
    /// escape sequences)
    /// @param text Text to measure
    /// @return Display width in columns
    unsigned calculate_display_width(const std::string& text) const;

    /// Wrap paragraph text to fit within width
    /// @param text Paragraph text
    /// @param width Maximum width in columns
    /// @return Vector of wrapped lines
    std::vector<std::string> wrap_paragraph(const std::string& text, unsigned width) const;

    /// Truncate text to fit within width
    /// @param text Text to truncate
    /// @param width Maximum width in columns
    /// @return Truncated text
    std::string truncate_text(std::string&& text, unsigned width) const;

    /// Get terminal dimensions
    /// @return Tuple of (width, height) in rows and columns
    std::tuple<unsigned, unsigned> get_terminal_dimensions();

    /// Move cursor up by specified number of lines
    /// @param lines Number of lines to move up
    void move_cursor_up(unsigned lines) const;

    /// Generate color escape sequence
    /// @param color RGB color
    /// @param foreground True for foreground, false for background
    /// @return Escape sequence string
    std::string color_escape(const terminal::info::color& color, bool foreground) const;
  };

} // namespace widget

#endif // _TEXTBOX_HH
