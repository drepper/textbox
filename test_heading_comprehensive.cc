#include "textbox.hh"
#include <print>

int main()
{
  auto term = terminal::info::alloc();

  ::std::println("=== Test 1: Starting with H2 ===");
  ::std::println("Expected: H2='1', H3='1.1', H2='2'\n");
  {
    widget::textbox box{*term, "Test1"};
    box.add_markdown("## First Section\n### Subsection\n## Second Section\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 2: Starting with H3 ===");
  ::std::println("Expected: H3='1', H4='1.1', H3='2'\n");
  {
    widget::textbox box{*term, "Test2"};
    box.add_markdown("### First\n#### Sub\n### Second\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 3: H2 first, then H1 appears ===");
  ::std::println("Expected: H2='1', H1='2', H2='2.1'\n");
  {
    widget::textbox box{*term, "Test3"};
    box.add_markdown("## Section\n# Chapter\n## Another Section\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 4: Normal H1 start ===");
  ::std::println("Expected: H1='1', H2='1.1', H3='1.1.1', H1='2'\n");
  {
    widget::textbox box{*term, "Test4"};
    box.add_markdown("# Chapter 1\n## Section\n### Subsection\n# Chapter 2\n");
    box.draw();
  }

  return 0;
}
