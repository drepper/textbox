# TextBox Widget

A C++ terminal text widget library for rendering formatted text with frames and styling in terminal emulators.

## Features

- **Frame Styles**: Choose from line drawing, background block, or no frame
- **Paragraph Management**: Support for both reflowing and fixed-width paragraphs
- **UTF-8 Support**: Full UTF-8 encoding with multi-column character support
- **Escape Sequences**: Handles ANSI escape sequences for colors and formatting
- **Customizable Colors**: Set foreground and background colors for text and frames
- **Margins**: Configurable left and right margins
- **Dynamic Rendering**: Automatically wraps text to fit terminal width
- **Automatic Scrolling**: Scrolls screen when widget exceeds terminal height
- **Smart Cursor Management**: Destructor positions cursor after widget automatically

## Building

Requirements:
- CMake 3.24+
- C++26 compatible compiler (GCC 15+ or Clang with C++26 support)
- libunistring development files
- termdetect library (included as subdirectory)

Build steps:
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Running Tests

```bash
cd build
make test
# Or run directly:
./textbox_test
```

## Usage Example

```cpp
#include "textbox.hh"

// Initialize terminal info
auto term = terminal::info::alloc();

// Create widget
widget::textbox box{*term, "My Widget"};

// Set frame style
box.set_frame(widget::textbox::frame_type::line);

// Customize colors
box.set_text_foreground(200, 200, 200);
box.set_text_background(20, 20, 40);
box.set_frame_foreground(100, 100, 200);

// Add reflowing text
box.add_text("This text will wrap automatically based on terminal width.\n");
box.add_text("New paragraphs are created with newlines.\n\n");

// Add fixed-format block
box.add_block({
    "Code blocks are fixed-width:",
    "    void example() {",
    "        // code here",
    "    }"
});

// Change title
box.set_title("Updated Title");

// When box goes out of scope, destructor automatically
// moves cursor to the line after the widget
```

**Note**: The widget always renders starting at the current cursor position. When the object is destroyed, the cursor is automatically moved to the line after the widget, making it easy to display multiple widgets sequentially.

## Frame Styles

### Line (default)
Uses box drawing characters:
```
╭─────────────╮
│ Content     │
╰─────────────╯
```

### Background
Uses block characters with background color:
```
▗▄▄▄▄▄▄▄▄▄▄▄▄▖
▐ Content    ▌
▝▀▀▀▀▀▀▀▀▀▀▀▀▘
```

### None
No frame, just content with optional background color.

## API Reference

### Constructor
- `textbox(const terminal::info& term, const std::string& name)` - Creates a new textbox widget

### Content Management
- `void add_text(const std::string& text)` - Add reflowing text
- `void add_block(const std::vector<std::string>& lines)` - Add fixed-format block

### Styling
- `void set_title(const std::string& new_title)` - Update widget title
- `void set_frame(frame_type ft)` - Set frame style (none, line, background)
- `void set_left_margin(unsigned margin)` - Set left margin in columns
- `void set_right_margin(unsigned margin)` - Set right margin in columns
- `void set_text_foreground(uint8_t r, uint8_t g, uint8_t b)` - Set text color
- `void set_text_background(uint8_t r, uint8_t g, uint8_t b)` - Set background color
- `void set_frame_foreground(uint8_t r, uint8_t g, uint8_t b)` - Set frame color

## Implementation Details

- Uses libunistring for proper UTF-8 character width calculation
- Handles escape sequences (CSI, OSC, etc.) when calculating display width
- Automatically wraps reflowing paragraphs at word boundaries
- Truncates fixed paragraphs to fit within frame
- Pads all lines to maintain consistent frame width
- Renders using modern C++ `std::print` for output

## License

See LICENSE file for details.

## Author

Ulrich Drepper
