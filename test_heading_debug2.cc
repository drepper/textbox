#include "textbox.hh"

#include <print>

int main()
{
  auto term = terminal::info::alloc();

  ::std::println("=== Test 1: H2 only ===\n");
  {
    widget::textbox box{*term, "Test1"};
    box.add_markdown("## Heading 2\n\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 2: H1 then H2 ===\n");
  {
    widget::textbox box{*term, "Test2"};
    box.add_markdown("# Heading 1\n\n## Heading 2\n\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 3: H2 then H1 (should renumber) ===\n");
  {
    widget::textbox box{*term, "Test3"};
    box.add_markdown("## Heading 2\n\n# Heading 1\n\n");
    box.draw();
  }

  return 0;
}
