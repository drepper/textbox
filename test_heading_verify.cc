#include "textbox.hh"

#include <cstdio>
#include <print>
#include <unistd.h>

int main()
{
  // Redirect stdout to a file to capture widget output
  FILE* outfile = ::fopen("heading_output.txt", "w");
  int saved_stdout = ::dup(STDOUT_FILENO);
  ::dup2(::fileno(outfile), STDOUT_FILENO);

  auto term = terminal::info::alloc();

  // Test 1: Only H2 and H3 (min should be 2)
  {
    widget::textbox box{*term, "Test1"};
    box.add_markdown("## First H2\n### Nested H3\n## Second H2\n");
    box.draw();
  }

  ::fprintf(outfile, "\n\n--- SEPARATOR ---\n\n");
  ::fflush(outfile);

  // Test 2: H2, H3, then H1 (min should be 1)
  {
    widget::textbox box{*term, "Test2"};
    box.add_markdown("## First H2\n### Nested H3\n# Top H1\n## Another H2\n");
    box.draw();
  }

  // Restore stdout and close file
  ::fflush(outfile);
  ::dup2(saved_stdout, STDOUT_FILENO);
  ::close(saved_stdout);
  ::fclose(outfile);

  // Now print the results to the terminal
  ::std::println("Test complete. Check heading_output.txt for results.");
  ::std::println("\nExpected for Test 1 (min=H2): H2='1', H3='1.1', H2='2'");
  ::std::println("Expected for Test 2 (min=H1): H2='1', H3='1.1', H1='2', H2='2.1'");

  return 0;
}
