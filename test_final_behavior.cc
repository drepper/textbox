#include "textbox.hh"
#include <print>

int main()
{
  auto term = terminal::info::alloc();

  ::std::println("=== Comprehensive Heading Numbering Test ===\n");

  ::std::println("Test 1: Document starting with H1");
  ::std::println("Expected: H1=¶, H2=¶ 1, H3=¶ 1.1, H2=¶ 2\n");
  {
    widget::textbox box{*term, "H1 Start"};
    box.add_markdown("# Chapter\n## Section\n### Subsection\n## Another Section\n");
    box.draw();
  }

  ::std::println("\n\nTest 2: Document starting with H2");
  ::std::println("Expected: H2=¶, H3=¶ 1, H2=¶\n");
  {
    widget::textbox box{*term, "H2 Start"};
    box.add_markdown("## Section\n### Subsection\n## Another Section\n");
    box.draw();
  }

  ::std::println("\n\nTest 3: H2 first, then H1 appears");
  ::std::println("Expected: Initial H2=¶, H3=¶ 1, then after H1: H2=¶ 1, H3=¶ 1.1, H1=¶, H2=¶ 1\n");
  {
    widget::textbox box{*term, "Dynamic"};
    box.add_markdown("## Section\n### Subsection\n# Chapter\n## Another Section\n");
    box.draw();
  }

  ::std::println("\n\nTest 4: Deep hierarchy");
  ::std::println("Expected: H1=¶, H2=¶ 1, H3=¶ 1.1, H4=¶ 1.1.1\n");
  {
    widget::textbox box{*term, "Deep"};
    box.add_markdown("# Book\n## Chapter\n### Section\n#### Subsection\n");
    box.draw();
  }

  return 0;
}
