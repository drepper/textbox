#include "textbox.hh"
#include <chrono>
#include <print>
#include <thread>

int main() {
  ::std::println("Starting cursor position test...");
  ::std::println("Line 2");
  ::std::println("Line 3");
  ::std::println("About to create first widget...");

  auto term = terminal::info::alloc(false);

  {
    ::std::println("\n--- Widget 1 ---");
    widget::textbox box1{*term, "First Widget"};
    box1.add_text("This is the first widget.\nIt has multiple lines.");
    ::std::this_thread::sleep_for(::std::chrono::seconds(2));
  }
  // Cursor should now be positioned after widget 1

  {
    ::std::println("\n--- Widget 2 ---");
    widget::textbox box2{*term, "Second Widget"};
    box2.add_text(
        "This is the second widget.\nIt should appear below the first.");
    ::std::this_thread::sleep_for(::std::chrono::seconds(2));
  }
  // Cursor should now be positioned after widget 2

  {
    ::std::println("\n--- Widget 3 ---");
    widget::textbox box3{*term, "Third Widget"};
    box3.set_frame(widget::textbox::frame_type::background);
    box3.add_text("Third widget with background frame.");
    ::std::this_thread::sleep_for(::std::chrono::seconds(2));
  }

  ::std::println("\nAll widgets complete!");
  ::std::println("Cursor should be positioned correctly after each widget.");

  return 0;
}
