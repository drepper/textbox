#include "textbox.hh"
#include <print>

int main()
{
  auto term = terminal::info::alloc();

  ::std::println("=== Test 1: H2 only (should have no number) ===");
  {
    widget::textbox box{*term, "Test1"};
    box.add_markdown("## First Section\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 2: H2, then H3 (H2 no number, H3 shows '1') ===");
  {
    widget::textbox box{*term, "Test2"};
    box.add_markdown("## Section\n### Subsection\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 3: H1 only (should have no number) ===");
  {
    widget::textbox box{*term, "Test3"};
    box.add_markdown("# Chapter\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 4: H1, then H2 (H1 no number, H2 shows '1') ===");
  {
    widget::textbox box{*term, "Test4"};
    box.add_markdown("# Chapter\n## Section\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 5: H1, H2, H3 (H1 no number, H2 shows '1', H3 shows '1.1') ===");
  {
    widget::textbox box{*term, "Test5"};
    box.add_markdown("# Chapter\n## Section\n### Subsection\n");
    box.draw();
  }

  ::std::println("\n\n=== Test 6: Multiple H2s with H3 (H2 no numbers, H3 shows '1' under first H2, '1' under second H2) ===");
  {
    widget::textbox box{*term, "Test6"};
    box.add_markdown("## First Section\n### Sub 1\n## Second Section\n### Sub 2\n");
    box.draw();
  }

  return 0;
}
