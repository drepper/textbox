#include "textbox.hh"

#include <print>

int main()
{
  auto term = terminal::info::alloc();

  // Test 1: Start with H2, should number as "1"
  ::std::println("=== Test 1: Starting with H2 ===");
  {
    widget::textbox box{*term, "Test 1"};
    box.add_markdown(R"(
## First Heading (H2)

Some text here.

### Subheading (H3)

More text.

## Second Heading (H2)

Final text.
)");
    box.draw();
  }

  ::std::println("\n\n=== Test 2: Starting with H3 ===");
  {
    widget::textbox box{*term, "Test 2"};
    box.add_markdown(R"(
### First Heading (H3)

Some text.

#### Subheading (H4)

More text.

### Second Heading (H3)

Final text.
)");
    box.draw();
  }

  ::std::println("\n\n=== Test 3: Mixed levels - H2 then H1 later ===");
  {
    widget::textbox box{*term, "Test 3"};
    box.add_markdown(R"(
## First Heading (H2)

Some text.

### Subheading (H3)

More text.

# Top Level (H1)

This should cause renumbering.

## Second H2

More content.
)");
    box.draw();
  }

  ::std::println("\n\n=== Test 4: Normal H1 start ===");
  {
    widget::textbox box{*term, "Test 4"};
    box.add_markdown(R"(
# Heading 1

Text.

## Heading 2

More text.

### Heading 3

Even more text.
)");
    box.draw();
  }

  return 0;
}
