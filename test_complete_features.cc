#include "textbox.hh"

int main()
{
  auto term = terminal::info::alloc();

  widget::textbox box{*term, "Complete Feature Test"};

  box.add_markdown(R"(
# 1. Introduction

This document tests all features.

## 1.1 Text Formatting

Regular text with **bold**, *italic*, `code`, and ~~strikethrough~~.

## 1.2 Lists with Formatting

Unordered list:
- Item with **bold text**
- Item with *italic text*
- Item with `inline code`
- Item with ~~strikethrough~~

Ordered list:
1. First item with **emphasis**
2. Second item with *styling*
3. Third item with `code`

## 1.3 Nested Lists

- Parent item with **bold**
  - Child item with *italic*
  - Another child with `code`
- Back to parent level

# 2. Code Blocks

Here's a code example:

```cpp
int main() {
    std::cout << "Hello!" << std::endl;
    return 0;
}
```

# 3. Mixed Content

Combining everything:
- **Bold** list item with `code` and *italic*
- Regular text
  - Nested with ~~strikethrough~~
)");

  box.draw();

  return 0;
}
