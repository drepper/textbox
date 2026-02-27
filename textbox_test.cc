#include "config.hh"
#include "textbox.hh"

#include <chrono>
#include <print>
#include <thread>

namespace {

void test_basic_widget() {
  ::std::println("\n=== Test 1: Basic Widget Creation ===");

  auto term = terminal::info::alloc(false);
  {
    widget::textbox box{*term, "Test Widget"};

    box.add_text("This is a simple text widget.");
    box.add_text(
        " It supports multiple paragraphs.\n\nThis is a second paragraph.");

    ::std::this_thread::sleep_for(::std::chrono::seconds(2));
  }
}

void test_block_content() {
  ::std::println("\n=== Test 2: Block Content ===");

  auto term = terminal::info::alloc();
  {
    widget::textbox box{*term, "Code Block Test"};

    box.add_text("Here is some code:\n");
    box.add_block(
        {"def hello_world():", "    print('Hello, World!')", "    return 42"});
    box.add_text("\nThis function demonstrates block formatting.");

    ::std::this_thread::sleep_for(::std::chrono::seconds(2));
  }
}

void test_frame_types() {
  ::std::println("\n=== Test 3: Frame Types ===");

  auto term = terminal::info::alloc();

  {
    ::std::println("\n--- Line Frame ---");
    widget::textbox box{*term, "Line Frame"};
    box.set_frame(widget::textbox::frame_type::line);
    box.add_text("This widget uses line drawing characters for the frame.");
    ::std::this_thread::sleep_for(::std::chrono::seconds(2));
  }

  {
    ::std::println("\n--- Background Frame ---");
    widget::textbox box{*term, "Background Frame"};
    box.set_frame(widget::textbox::frame_type::background);
    box.add_text("This widget uses block characters with background color.");
    ::std::this_thread::sleep_for(::std::chrono::seconds(2));
  }

  {
    ::std::println("\n--- No Frame ---");
    widget::textbox box{*term, "No Frame"};
    box.set_frame(widget::textbox::frame_type::none);
    box.add_text("This widget has no frame at all.");
    ::std::this_thread::sleep_for(::std::chrono::seconds(2));
  }
}

void test_title_change() {
  ::std::println("\n=== Test 4: Title Change ===");

  auto term = terminal::info::alloc();
  {
    widget::textbox box{*term, "Original Title"};

    box.add_text("Watch the title change...");
    ::std::this_thread::sleep_for(::std::chrono::seconds(1));

    box.set_title("New Title!");
    ::std::this_thread::sleep_for(::std::chrono::seconds(2));
  }
}

void test_margins() {
  ::std::println("\n=== Test 5: Margins ===");

  auto term = terminal::info::alloc();
  {
    widget::textbox box{*term, "Margins Test"};

    box.set_left_margin(5);
    box.set_right_margin(5);
    box.add_text("This widget has 5-column margins on both sides.");

    ::std::this_thread::sleep_for(::std::chrono::seconds(2));
  }
}

void test_colors() {
  ::std::println("\n=== Test 6: Custom Colors ===");

  auto term = terminal::info::alloc();
  {
    widget::textbox box{*term, "Color Test"};

    box.set_text_foreground(100, 200, 255); // Light blue
    box.set_text_background(0, 20, 40);     // Dark blue
    box.set_frame_foreground(0, 100, 200);  // Medium blue

    box.add_text("This widget uses custom blue color scheme.");

    ::std::this_thread::sleep_for(::std::chrono::seconds(2));
  }
}

void test_utf8_multibyte() {
  ::std::println("\n=== Test 7: UTF-8 Multi-byte Characters ===");

  auto term = terminal::info::alloc();
  {
    widget::textbox box{*term, "Unicode Test \u2603"};

    box.add_text("Testing various Unicode characters:\n");
    box.add_text("Emoji: \U0001f600 \u2665 \U0001f680\n");
    box.add_text("CJK: \u3042\u3044\u3046 ");
    box.add_text("\u65e5\u672c\u8a9e\n");
    box.add_text("Symbols: \u2190 \u2713 \u00d7");

    ::std::this_thread::sleep_for(::std::chrono::seconds(3));
  }
}

void test_long_text_wrapping() {
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
        "of multi-byte character sequences.");

    ::std::this_thread::sleep_for(::std::chrono::seconds(3));
  }
}

void test_escape_sequences() {
  ::std::println("\n=== Test 9: Escape Sequences in Content ===");

  auto term = terminal::info::alloc();
  {
    widget::textbox box{*term, "Escape Test"};

    box.add_text("This text contains \033[1mbold\033[0m and "
                 "\033[4munderlined\033[0m formatting.\n");
    box.add_text(
        "Colors: \033[31mred\033[0m \033[32mgreen\033[0m \033[34mblue\033[0m");

    ::std::this_thread::sleep_for(::std::chrono::seconds(3));
  }
}

void test_mixed_content() {
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

    box.add_block({"And a code block:", "    int main() {", "        return 0;",
                   "    }"});

    box.add_text("\nWith Unicode: \u2713 \u2728 \U0001f680");

    ::std::this_thread::sleep_for(::std::chrono::seconds(4));
  }
}

} // anonymous namespace

int main() {
  ::std::println("TextBox Widget Test Suite");
  ::std::println("Version: {}", PROJECT_VERSION);
  ::std::println("URL: {}", PROJECT_HOMEPAGE_URL);

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

    ::std::println("\n\nAll tests completed successfully!");
    return 0;
  } catch (const std::exception &e) {
    ::std::println(stderr, "Test failed with exception: {}", e.what());
    return 1;
  }
}
