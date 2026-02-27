# TextBox Widget - Recent Changes

## 2026-02-27 (Update 5): Title Colors and Frame Background Fix

### Color Handling Improvements

Added separate title colors and fixed background color handling for frame characters.

**Changes:**
1. Added `title_fg` and `title_bg` color members
2. Initialize from `term_info.default_foreground` and `term_info.default_background`
3. Title text now uses these colors in both frame types
4. Fixed: Frame characters no longer get background color in background frame type
5. Background color only applied to content area, not frame borders

**Color Usage:**
- **Title**: Uses terminal defaults (title_fg, title_bg)
- **Frame**: Foreground only (frame_fg), default background
- **Content**: text_fg and text_bg (background only when frame=background or none)

**Key Fix:**
Background frame type now correctly applies background only to content area between frame characters, not to the frame characters themselves.

See `TITLE_COLORS_UPDATE.md` for complete details.

---

## 2026-02-27 (Update 4): Relative Cursor Movement

### Major Refactoring

Converted from absolute cursor positioning to relative cursor movement. Widget no longer tracks or queries cursor position.

**Changes:**
1. Constructor outputs `\r` to ensure starting at column 1
2. Removed all absolute cursor positioning code
3. Use `\033[{n}A` (Cursor Up) for re-rendering
4. Each line naturally ends with `\n` - cursor advances automatically
5. Destructor does nothing - cursor already positioned correctly

**Removed:**
- `get_cursor_position()` - No longer queries DSR
- `move_cursor(row, col)` - No absolute positioning
- `scroll_up()` - Terminal handles naturally
- `initial_row`, `initial_col` - No coordinate tracking

**Added:**
- `move_cursor_up(lines)` - Relative movement for re-rendering

**Benefits:**
- Simpler code (~40 lines removed)
- No DSR queries needed
- More robust - works regardless of terminal state
- Terminal handles scrolling naturally
- Clean semantics - widget just outputs content

**How It Works:**
1. Constructor: `\r` → column 1
2. First render: Output content (each line ends with `\n`)
3. Re-render: Move up by height, `\r`, redraw
4. Destructor: Nothing needed (cursor already positioned)

See `RELATIVE_CURSOR_MOVEMENT.md` for complete details.

---

## 2026-02-27 (Update 3): Switched to ::write() and ::writev()

### Major Refactoring

Replaced all uses of `::std::print()` and stdout with direct terminal file descriptor writes using `::write()` and `::writev()`.

**Changes:**
1. Removed `#include <print>`, added `#include <sys/uio.h>`
2. Created helper functions:
   - `write_str(fd, str)` - Write single string
   - `writev_strs(fd, {...})` - Write multiple strings in one system call
3. All rendering now uses `term_info.get_fd()` for output
4. Removed all `::std::fflush(stdout)` calls

**Benefits:**
- ~7x fewer system calls (one `writev()` per line vs multiple `write()` calls)
- Consistent file descriptor usage throughout
- No buffering concerns
- Atomic multi-segment writes
- **Zero compiler warnings** (fixed multi-character constant issues)

**UTF-8 Handling:**
- Pre-build repeated Unicode characters as UTF-8 byte sequences
- Example: U+2500 (─) = `\xe2\x94\x80`

**Example:**
```cpp
// Before: 6+ system calls
::std::print("{}", color1);
::std::print("\u2502");
::std::print("{}", line);
::std::fflush(stdout);

// After: 1 system call
writev_strs(fd, {color1, "\u2502", line, "\033[0m\n"});
```

See `WRITE_WRITEV_CHANGES.md` for full details.

---

## 2026-02-27 (Update 2): Fixed Cursor Position Detection

### Critical Bug Fix

Fixed issue where widget was always drawn at line 1 of the terminal instead of at the current cursor position.

**Root Cause:**
- DSR (Device Status Report) query was sent to stdout but response was read from terminal fd
- stdout was not flushed before querying cursor position

**Fix:**
1. Changed DSR query to use terminal fd for both write and read:
   ```cpp
   const char* dsr = "\033[6n";
   ::write(term_info.get_fd(), dsr, 4);  // Was: ::std::print("\033[6n")
   ```

2. Added stdout flush before querying position in constructor:
   ```cpp
   ::std::fflush(stdout);  // Ensure cursor at actual position
   auto pos = get_cursor_position();
   ```

**Result:**
- Widget now correctly renders at current cursor position
- Cursor properly positioned after widget destruction
- Multiple widgets display sequentially without overlap

**Test:**
Created `test_cursor.cc` to verify cursor positioning works correctly.

---

## 2026-02-27 (Update 1): Scrolling and Cursor Management

### Changes Made

#### 1. Widget Position Always Starts at Current Cursor Line
- Widget now captures cursor position when created and always renders starting from that line
- First line of widget is guaranteed to be at the cursor position when the object was created

#### 2. Automatic Screen Scrolling
- Widget now calculates total height needed before rendering
- When widget would extend beyond terminal height, screen scrolls up automatically
- Scroll amount calculated as: `(initial_row + needed_height) - term_height`
- Widget's `initial_row` adjusted after scrolling to maintain proper position

#### 3. Destructor with Cursor Positioning
- Added destructor `~textbox()` that moves cursor to line after widget
- Cursor positioned at `initial_row + widget_height` on column 1
- Ensures clean separation between consecutive widgets in tests

#### 4. Terminal Dimensions Tracking
- Changed from `get_terminal_width()` to `get_terminal_dimensions()`
- Now returns `std::tuple<unsigned, unsigned>` of (height, width)
- Default fallback is 24×80 if geometry detection fails

#### 5. Scroll Implementation
- Added `scroll_up(unsigned lines)` method
- Uses ANSI escape sequence `\033[{lines}S` to scroll screen
- Called automatically when widget needs more room

#### 6. Widget Height Tracking
- Added `widget_height` member variable
- Updated during render to track actual rendered height
- Used by destructor to position cursor correctly

### Header Changes (textbox.hh)

```cpp
// Added destructor
~textbox();

// Changed method signature
std::tuple<unsigned, unsigned> get_terminal_dimensions() const;

// Added scroll method
void scroll_up(unsigned lines) const;

// Added member variable
unsigned widget_height = 0;
```

### Implementation Changes (textbox.cc)

#### Destructor
```cpp
textbox::~textbox()
{
  // Move cursor to line after widget
  if (widget_height > 0) {
    move_cursor(initial_row + widget_height, 1);
    ::std::fflush(stdout);
  }
}
```

#### Render Method Updates
- Pre-calculates total widget height by counting:
  - Top frame (if present)
  - All content lines (wrapped or fixed)
  - Inter-paragraph spacing
  - Bottom frame (if present)
- Checks if scrolling needed: `initial_row + needed_height > term_height`
- Scrolls and adjusts position if needed
- Stores calculated height in `widget_height`

#### Scroll Implementation
```cpp
void textbox::scroll_up(unsigned lines) const
{
  ::std::print("\033[{}S", lines);
  ::std::fflush(stdout);
}
```

### Test Changes (textbox_test.cc)

All test functions updated to use scoped blocks for textbox objects:

```cpp
void test_function()
{
  auto term = terminal::info::alloc();
  {
    widget::textbox box{*term, "Test"};
    // ... test code ...
  }
  // Destructor called here, cursor moves to next line
}
```

**Tests Updated:**
1. `test_basic_widget()`
2. `test_block_content()`
3. `test_frame_types()` (already had scopes for sub-tests)
4. `test_title_change()`
5. `test_margins()`
6. `test_colors()`
7. `test_utf8_multibyte()`
8. `test_long_text_wrapping()`
9. `test_escape_sequences()`
10. `test_mixed_content()`

### Benefits

1. **No Manual Cursor Management**: Tests don't need to manually move cursor between widgets
2. **Automatic Scrolling**: Widgets work correctly even when terminal runs out of vertical space
3. **Clean Test Output**: Each widget followed by proper cursor positioning
4. **Consistent Behavior**: Widget always renders from its initial cursor position
5. **RAII Pattern**: Destructor ensures cleanup happens automatically

### Build Status

✅ **Successfully Built**
- Compiler: GCC 15.2.1
- Standard: C++26
- Minor warning: -Weffc++ suggests initializing `paragraphs` in member initializer (cosmetic only)
- Library size: 3.0 MB
- Test executable size: 2.8 MB

### Technical Details

#### Height Calculation Algorithm
1. Count top frame (1 line if frame != none)
2. For each paragraph:
   - If reflow: count wrapped lines
   - If fixed: count newlines in content
   - Add 1 for inter-paragraph spacing (except last)
3. Count bottom frame (1 line if frame != none)

#### Scroll Decision
```cpp
if (initial_row + needed_height > term_height) {
    unsigned scroll_lines = (initial_row + needed_height) - term_height;
    scroll_up(scroll_lines);
    initial_row -= scroll_lines;
}
```

This ensures the widget fits on screen while maintaining its logical starting position.

### Backward Compatibility

- No breaking changes to public API
- Existing code continues to work
- Additional destructor behavior is transparent
- Scrolling happens automatically when needed

### Testing Recommendations

When testing, observe:
1. Widgets appear at cursor position
2. Cursor moves to next line after each widget
3. Screen scrolls when widget would extend beyond terminal
4. Multiple widgets display cleanly in sequence
5. No overlap between consecutive widgets
