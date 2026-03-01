#include "textbox.hh"
#include <print>

int main()
{
  auto term = terminal::info::alloc();

  ::std::println("=== Test: List items with inline formatting ===\n");

  widget::textbox box{*term, "List Formatting Test"};

  box.add_markdown(R"(
# Features

## Unordered List

- This is **bold text** in a list item
- This has *italic text* here
- And this has `inline code` markup
- Finally ~~strikethrough~~ text

## Ordered List

1. **Important**: This entire item is bold
2. You can use *emphasis* in lists
3. Even `code snippets` work
4. And ~~mistakes~~ can be crossed out

## Mixed Formatting

- Combine **bold** and *italic* text
- Use `code` with **bold** together
- All ~~old~~ text is **new**
)");

  box.draw();

  return 0;
}
