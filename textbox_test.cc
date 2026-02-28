#include "textbox.hh"

#include <chrono>
#include <print>
#include <thread>

namespace {

  void test_basic_widget()
  {
    ::std::println("\n=== Test 1: Basic Widget Creation ===");

    auto term = terminal::info::alloc(false);
    {
      widget::textbox box{*term, "Test Widget"};

      box.add_text("This is a simple text widget.");
      box.add_text(" It supports multiple paragraphs.\n\nThis is a second paragraph.");

      // ::std::this_thread::sleep_for(::std::chrono::seconds(2));
    }
  }

  void test_block_content()
  {
    ::std::println("\n=== Test 2: Block Content ===");

    auto term = terminal::info::alloc();
    {
      widget::textbox box{*term, "Code Block Test"};

      box.add_text("Here is some code:\n");
      box.add_block({"def hello_world():", "    print('Hello, World!')", "    return 42"});
      box.add_text("\nThis function demonstrates block formatting.");

      // ::std::this_thread::sleep_for(::std::chrono::seconds(2));
    }
  }

  void test_frame_types()
  {
    ::std::println("\n=== Test 3: Frame Types ===");

    auto term = terminal::info::alloc();

    {
      ::std::println("\n--- Line Frame ---");
      widget::textbox box{*term, "Line Frame"};
      box.set_frame(widget::textbox::frame_type::line);
      box.add_text("This widget uses line drawing characters for the frame.");

      // ::std::this_thread::sleep_for(::std::chrono::seconds(2));
    }

    {
      ::std::println("\n--- Background Frame ---");
      widget::textbox box{*term, "Background Frame"};
      box.set_frame(widget::textbox::frame_type::background);
      box.add_text("This widget uses block characters with background color.");

      // ::std::this_thread::sleep_for(::std::chrono::seconds(2));
    }

    {
      ::std::println("\n--- No Frame ---");
      widget::textbox box{*term, "No Frame"};
      box.set_frame(widget::textbox::frame_type::none);
      box.add_text("This widget has no frame at all.");

      // ::std::this_thread::sleep_for(::std::chrono::seconds(2));
    }
  }

  void test_title_change()
  {
    ::std::println("\n=== Test 4: Title Change ===");

    auto term = terminal::info::alloc();
    {
      widget::textbox box{*term, "Original Title"};

      box.add_text("Watch the title change...");
      ::std::this_thread::sleep_for(::std::chrono::seconds(1));

      box.set_title("New Title!");

      // ::std::this_thread::sleep_for(::std::chrono::seconds(2));
    }
  }

  void test_margins()
  {
    ::std::println("\n=== Test 5: Margins ===");

    auto term = terminal::info::alloc();
    {
      widget::textbox box{*term, "Margins Test"};

      box.set_left_margin(5);
      box.set_right_margin(5);
      box.add_text("This widget has 5-column margins on both sides.");

      // ::std::this_thread::sleep_for(::std::chrono::seconds(2));
    }
  }

  void test_colors()
  {
    ::std::println("\n=== Test 6: Custom Colors ===");

    auto term = terminal::info::alloc();
    {
      widget::textbox box{*term, "Color Test"};

      box.set_text_foreground(100, 200, 255); // Light blue
      box.set_text_background(0, 20, 40);     // Dark blue
      box.set_frame_foreground(0, 100, 200);  // Medium blue

      box.add_text("This widget uses custom blue color scheme.");

      // ::std::this_thread::sleep_for(::std::chrono::seconds(2));
    }
  }

  void test_utf8_multibyte()
  {
    ::std::println("\n=== Test 7: UTF-8 Multi-byte Characters ===");

    auto term = terminal::info::alloc();
    {
      widget::textbox box{*term, "Unicode Test \u2603"};

      box.add_text("Testing various Unicode characters:\n");
      box.add_text("Emoji: \U0001f600 \u2665 \U0001f680\n");
      box.add_text("CJK: \u3042\u3044\u3046 ");
      box.add_text("\u65e5\u672c\u8a9e\n");
      box.add_text("Symbols: \u2190 \u2713 \u00d7");

      // ::std::this_thread::sleep_for(::std::chrono::seconds(3));
    }
  }

  void test_long_text_wrapping()
  {
    ::std::println("\n=== Test 8: Long Text Wrapping ===");

    auto term = terminal::info::alloc();
    {
      widget::textbox box{*term, "Text Wrapping"};

      box.add_text(
          "This is a very long paragraph that should wrap across multiple lines "
          "when it exceeds the width of the terminal window. The wrapping "
          "algorithm "
          "should try to break at word boundaries when possible to maintain "
          "readability. "
          "It should also handle UTF-8 characters correctly and not break in the "
          "middle "
          "of multi-byte character sequences."
      );

      // ::std::this_thread::sleep_for(::std::chrono::seconds(3));
    }
  }

  void test_escape_sequences()
  {
    ::std::println("\n=== Test 9: Escape Sequences in Content ===");

    auto term = terminal::info::alloc();
    {
      widget::textbox box{*term, "Escape Test"};

      box.add_text(
          "This text contains \033[1mbold\033[0m and "
          "\033[4munderlined\033[0m formatting.\n"
      );
      box.add_text("Colors: \033[31mred\033[0m \033[32mgreen\033[0m \033[34mblue\033[0m");

      // ::std::this_thread::sleep_for(::std::chrono::seconds(3));
    }
  }

  void test_mixed_content()
  {
    ::std::println("\n=== Test 10: Mixed Content ===");

    auto term = terminal::info::alloc();
    {
      widget::textbox box{*term, "Mixed Content Demo"};

      box.set_frame(widget::textbox::frame_type::background);
      box.set_text_foreground(200, 200, 200);
      box.set_text_background(40, 20, 40);
      box.set_frame_foreground(80, 40, 80);

      box.add_text("This demonstrates a complete widget with:\n");
      box.add_text("- Background frame\n");
      box.add_text("- Custom colors\n");
      box.add_text("- Multiple paragraphs\n\n");

      box.add_block({"And a code block:", "    int main() {", "        return 0;", "    }"});

      box.add_text("\nWith Unicode: \u2713 \u2728 \U0001f680");

      // ::std::this_thread::sleep_for(::std::chrono::seconds(4));
    }
  }

  void test_markdown()
  {
    ::std::println("\n=== Test 11: Markdown Formatting ===");

    auto term = terminal::info::alloc();
    {
      widget::textbox box{*term, "Markdown Demo"};

      box.add_markdown("# Heading Level 1\n\n");
      box.add_markdown("## Heading Level 2\n\n");
      box.add_markdown("### Heading Level 3\n\n");
      box.add_markdown("#### Heading Level 4\n\n");
      box.add_markdown("##### Heading Level 5\n\n");
      box.add_markdown("###### Heading Level 6\n\n");

      box.add_markdown(
          "This paragraph demonstrates **bold text**, *italic text*, "
          "_italic with underscores_, ~~strikethrough text~~, and `inline code`.\n\n"
      );

      box.add_markdown("Here is a C++ code example:\n\n");
      box.add_markdown(R"(```cpp
#include <iostream>

int main() {
    std::cout << "Hello, World!\n";
    return 0;
}
```

)");

      box.add_markdown("And here is a Python example:\n\n");
      box.add_markdown(R"(```python
def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)

print(fibonacci(10))
```

)");

      box.add_markdown(
          "You can **mix** *different* ~~styles~~ with `code` in the "
          "same paragraph.\n\n"
      );

      box.add_markdown("Code blocks can also be used without language specification:\n\n");
      box.add_markdown(R"(```
This is plain text in a code block.
It will use the default code styling.
```
)");

      box.add_markdown("- itemize item #1\n");
      box.add_markdown("- itemize item #2\n");
      box.add_markdown("  - itemize item #2.1\n");
      box.add_markdown("- itemize item #3\n\n");

      box.add_markdown("1. enumerate item #1\n");
      box.add_markdown("1. enumerate item #2\n");
      box.add_markdown("1. enumerate item #3\n\n");

      box.add_markdown("> This is a blockquote.\n");
      box.add_markdown(R"*(> Want to write on a new line with space between?
>
> > And nested? No problem at all.\n)*");
      box.add_markdown("> >\n");
      box.add_markdown("> > > PS. you can **style** your text _as you want_.\n");

      ::std::this_thread::sleep_for(::std::chrono::seconds(1));
    }
  }

  void test_lists()
  {
    ::std::println("\n=== Test 12: Lists (Itemized and Enumerated) ===");

    auto term = terminal::info::alloc();
    {
      widget::textbox box{*term, "Lists Demo"};

      box.add_markdown("# Lists\n\n");

      box.add_markdown("## Unordered List\n\n");
      box.add_markdown("- First item\n");
      box.add_markdown("- Second item\n");
      box.add_markdown("- Third item\n\n");

      box.add_markdown("## Ordered List\n\n");
      box.add_markdown("1. First item\n");
      box.add_markdown("2. Second item\n");
      box.add_markdown("3. Third item\n\n");

      box.add_markdown("## Nested Lists\n\n");
      box.add_markdown("- Level 1 item 1\n");
      box.add_markdown("  - Level 2 item 1\n");
      box.add_markdown("    - Level 3 item 1\n");
      box.add_markdown("    - Level 3 item 2\n");
      box.add_markdown("  - Level 2 item 2\n");
      box.add_markdown("- Level 1 item 2\n\n");

      box.add_markdown("## Mixed Nested Lists\n\n");
      box.add_markdown("1. First ordered item\n");
      box.add_markdown("   - Unordered subitem\n");
      box.add_markdown("   - Another unordered subitem\n");
      box.add_markdown("2. Second ordered item\n");
      box.add_markdown("   1. Ordered subitem\n");
      box.add_markdown("   2. Another ordered subitem\n");
      box.add_markdown("3. Third ordered item\n\n");

      box.add_markdown("## Long Text in Lists\n\n");
      box.add_markdown(
          "- This is a very long list item that should wrap across multiple lines "
          "when it exceeds the width of the terminal. The continuation lines should "
          "be properly indented to align with the start of the text.\n"
      );
      box.add_markdown(
          "- Another long item demonstrating proper text wrapping and indentation "
          "for readability in itemized lists.\n\n"
      );

      ::std::this_thread::sleep_for(::std::chrono::seconds(1));
    }
  }

  void test_blockquotes()
  {
    ::std::println("\n=== Test 13: Blockquotes ===");

    auto term = terminal::info::alloc();
    {
      widget::textbox box{*term, "Blockquotes Demo"};

      box.add_markdown("# Blockquotes\n\n");

      box.add_markdown("## Simple Blockquote\n\n");
      box.add_markdown("> This is a simple blockquote.\n");
      box.add_markdown("> It can span multiple lines.\n");
      box.add_markdown("> Each line starts with >.\n\n");

      box.add_markdown("## Nested Blockquotes\n\n");
      box.add_markdown("> Level 1 blockquote\n");
      box.add_markdown("> > Level 2 blockquote (with spaces)\n");
      box.add_markdown("> > > Level 3 blockquote (with spaces)\n");
      box.add_markdown(">> Back to level 2 (without spaces)\n");
      box.add_markdown("> Back to level 1\n\n");

      box.add_markdown("## Blockquote with Long Text\n\n");
      box.add_markdown(
          "> This is a longer blockquote that demonstrates text wrapping within a "
          "blockquote. The text should wrap properly while maintaining the quote "
          "indentation and marker.\n\n"
      );

      box.add_markdown("## Blockquote with Formatting\n\n");
      box.add_markdown("> This has **bold** and *italic* text.\n");
      box.add_markdown("> Also _italic with underscores_, `inline code` and ~~strikethrough~~.\n\n");

      box.add_markdown("## Blockquote with Lists\n\n");
      box.add_markdown("> Here's a list in a blockquote:\n");
      box.add_markdown("> - First item\n");
      box.add_markdown(
          "> - This is a long second item that should wrap properly within the "
          "blockquote with correct indentation for continuation lines.  Add a few more words to make sure "
          "there is enough text to fill a line.\n"
      );
      box.add_markdown(">   - Nested item\n");
      box.add_markdown("> - Third item\n\n");

      box.add_markdown("## Nested Blockquote with List\n\n");
      box.add_markdown("> Level 1\n");
      box.add_markdown("> > Level 2 with list:\n");
      box.add_markdown("> > 1. First\n");
      box.add_markdown("> > 2. Second\n");
      box.add_markdown("> Back to level 1\n\n");

      ::std::this_thread::sleep_for(::std::chrono::seconds(1));
    }
  }

} // anonymous namespace

int main()
{
  ::std::println("TextBox Widget Test Suite");

  try {
    test_basic_widget();
    test_block_content();
    test_frame_types();
    test_title_change();
    test_margins();
    test_colors();
    test_utf8_multibyte();
    test_long_text_wrapping();
    test_escape_sequences();
    test_mixed_content();
    test_markdown();
    test_lists();
    test_blockquotes();

    ::std::println("\n\nAll tests completed successfully!");
    return 0;
  }
  catch (const std::exception& e) {
    ::std::println(stderr, "Test failed with exception: {}", e.what());
    return 1;
  }
}
