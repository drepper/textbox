# Unified Diff Support in Textbox Widget

The textbox widget supports displaying unified diffs with syntax highlighting and color-coded changes.

## Features

1. **Filename Display**: Shows the filename on its own line before the diff content
2. **Syntax Highlighting**: Uses the source-highlight library to highlight code content
3. **Language Detection**: Automatically detects file type from file extensions
4. **Color-Coded Changes**:
   - Removed lines: dark red background (0x3d0100)
   - Added lines: dark green background (0x022800)
   - Context lines: normal background
5. **Line Numbers**: Shows old and new file line numbers in aligned columns
6. **Visual Separators**: Uses Unicode characters for clean formatting:
   - `⋮` (VERTICAL ELLIPSIS) between old and new line numbers
   - `│` (BOX DRAWINGS LIGHT VERTICAL) before content
7. **Automatic Truncation**: Long lines are truncated to fit terminal width

## Format

Each diff line is formatted as:

```
 old# ⋮ new# │ content
```

Where:
- `old#` - Line number in original file (dynamically sized, empty for added lines)
- `new#` - Line number in new file (dynamically sized, empty for removed lines)
- `⋮` - Visual separator (VERTICAL ELLIPSIS) between line numbers
- `│` - Separator (BOX DRAWINGS LIGHT VERTICAL) before content
- `content` - The actual line content with syntax highlighting

**Column Width**: The width of each line number column is determined by the number of digits in the maximum line number for that file. Numbers are right-aligned within their column width. This makes the columns as narrow as possible while maintaining proper alignment.

**Spacing**: Each line number has one space before and one space after it:
- Leading space at start of line
- Line number (right-aligned within column width)
- Space before separator (⋮)
- Space after separator
- Line number (right-aligned within column width)
- Space before separator (│)
- Content

The complete format is:
- Context line: ` {grey}{old:>width} {grey}⋮ {new:>width} {grey}│ {content}`
- Removed line: ` {grey}{red}{old:>width} {grey}⋮ {empty:width} {grey}│ {white}{dark_red_bg}{content}`
- Added line: ` {grey}{empty:width} {grey}⋮ {green}{new:>width} {grey}│ {white}{dark_green_bg}{content}`

Example with small line numbers (width=2):
```
  1 ⋮  1 │ context line (spaces: " ", " 1", " ⋮ ", " 1", " │ ")
 10 ⋮ 10 │ context line (spaces: " ", "10", " ⋮ ", "10", " │ ")
 11 ⋮    │ removed line (spaces: " ", "11", " ⋮ ", "  ", " │ ")
    ⋮ 11 │ added line   (spaces: " ", "  ", " ⋮ ", "11", " │ ")
```

Example with larger line numbers (width=4):
```
    1 ⋮    1 │ context line
 1234 ⋮ 1234 │ context line
 1235 ⋮      │ removed line
      ⋮ 1235 │ added line
```

## Example

```cpp
widget::textbox tb{term, "Diff Viewer"};

std::string diff = R"(--- a/example.cpp
+++ b/example.cpp
@@ -10,7 +10,8 @@ int calculate(int x, int y)
 {
   if (x < 0)
     return -1;
-  return x * y;
+  // Fixed: handle overflow
+  return x + y;
 }
)";

tb.add_udiff(diff);
```

This renders with:
- First line: `example.cpp` (the filename)
- Line `return x * y;` shown with red background and only old line number
- Lines `// Fixed: handle overflow` and `return x + y;` shown with green background and only new line numbers
- Context lines shown with both old and new line numbers

The filename is automatically extracted from the diff headers (`--- a/example.cpp` and `+++ b/example.cpp`), with the `a/` or `b/` prefix removed.

## Color Scheme

### Line Numbers
- Regular numbers: Dark grey (128, 128, 128)
- Removed line numbers: Red (133, 0, 0) / 0x850000
- Added line numbers: Green (0, 135, 0) / 0x008700

### Backgrounds
- Removed lines: Dark red (61, 1, 0) / 0x3d0100
- Added lines: Dark green (2, 40, 0) / 0x022800
- Context lines: Widget default background

## Supported Languages

Language detection is based on file extensions:
- C++: .cpp, .cc, .cxx, .c++
- C: .c, .h
- Python: .py
- JavaScript: .js
- Rust: .rs
- Go: .go
- Java: .java
- Shell: .sh, .bash

## Implementation Details

- Parses unified diff format (lines starting with `---`, `+++`, `@@`, `-`, `+`, ` `)
- Tracks line numbers separately for old and new files
- Uses source-highlight library for syntax highlighting when language is detected
- Preserves background colors when applying syntax highlighting
- Renders as fixed (non-reflowing) paragraphs
