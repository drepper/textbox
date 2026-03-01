#include "textbox.hh"

#include <print>

int main()
{
  auto term = terminal::info::alloc();

  ::std::println("Test 1: Starting with H2, then H3, then H2");
  ::std::println("Expected: H2='¶ 1', H3='¶ 1.1', H2='¶ 2'");
  {
    widget::textbox box{*term, "Test 1"};
    box.add_markdown("## First H2\n### Nested H3\n## Second H2\n");
    box.draw();
  }

  ::std::println("\n\nTest 2: Starting with H3, then H4, then H3");
  ::std::println("Expected: H3='¶ 1', H4='¶ 1.1', H3='¶ 2'");
  {
    widget::textbox box{*term, "Test 2"};
    box.add_markdown("### First H3\n#### Nested H4\n### Second H3\n");
    box.draw();
  }

  ::std::println("\n\nTest 3: H2, H3, then H1 appears");
  ::std::println("Expected: H2='¶ 1.1', H3='¶ 1.1.1', H1='¶ 2', H2='¶ 2.1'");
  {
    widget::textbox box{*term, "Test 3"};
    box.add_markdown("## First H2\n### Nested H3\n# Top H1\n## Another H2\n");
    box.draw();
  }

  ::std::println("\n\nTest 4: Normal H1, H2, H3");
  ::std::println("Expected: H1='¶ 1', H2='¶ 1.1', H3='¶ 1.1.1', H1='¶ 2'");
  {
    widget::textbox box{*term, "Test 4"};
    box.add_markdown("# First H1\n## Nested H2\n### Nested H3\n# Second H1\n");
    box.draw();
  }

  return 0;
}
