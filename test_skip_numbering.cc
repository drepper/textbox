#include "textbox.hh"
#include <print>

int main()
{
  auto term = terminal::info::alloc();

  ::std::println("=== Test: Manual numbering should be stripped ===\n");

  ::std::println("Test 1: Headings with manual numbers");
  ::std::println("Input: '# 1. Chapter', '## 1.1 Section', '## 1.2 Another'");
  ::std::println("Expected: '¶ Chapter', '¶ 1 Section', '¶ 2 Another'\n");
  {
    widget::textbox box{*term, "Manual Numbers"};
    box.add_markdown("# 1. Chapter\n## 1.1 Section\n## 1.2 Another\n");
    box.draw();
  }

  ::std::println("\n\nTest 2: Some with numbers, some without");
  ::std::println("Input: '# Chapter', '## 2.1. Introduction', '## Background'");
  ::std::println("Expected: '¶ Chapter', '¶ 1 Introduction', '¶ 2 Background'\n");
  {
    widget::textbox box{*term, "Mixed"};
    box.add_markdown("# Chapter\n## 2.1. Introduction\n## Background\n");
    box.draw();
  }

  ::std::println("\n\nTest 3: Just dots and spaces");
  ::std::println("Input: '## 123. Title', '### 1.2.3. Subtitle'");
  ::std::println("Expected: '¶ Title', '¶ 1 Subtitle'\n");
  {
    widget::textbox box{*term, "Complex"};
    box.add_markdown("## 123. Title\n### 1.2.3. Subtitle\n");
    box.draw();
  }

  return 0;
}
