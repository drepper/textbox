#include "textbox.hh"
#include <print>

int main()
{
  auto term = terminal::info::alloc();

  ::std::println("=== Test 1: H2 first (should show NO number) ===");
  {
    widget::textbox box{*term, "Test1"};
    box.add_markdown("## First H2\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 2: H2, then H3 ===");
  ::std::println("Expected: H2=no number, H3='¶ 1'");
  {
    widget::textbox box{*term, "Test2"};
    box.add_markdown("## Section\n### Subsection\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 3: H2, H3, then H1 appears ===");
  ::std::println("Expected before H1: H2=no number, H3='¶ 1'");
  ::std::println("Expected after H1: H2='¶ 1', H3='¶ 1.1', H1=no number");
  {
    widget::textbox box{*term, "Test3"};
    box.add_markdown("## Section\n### Subsection\n# Chapter\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 4: H2, then H1, then another H2 ===");
  ::std::println("Expected: First H2='¶ 1', H1=no number, Second H2='¶ 1'");
  {
    widget::textbox box{*term, "Test4"};
    box.add_markdown("## First Section\n# Chapter\n## Second Section\n");
    box.draw();
  }

  return 0;
}
