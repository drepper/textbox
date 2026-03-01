#include "textbox.hh"

#include <cstdio>
#include <print>
#include <unistd.h>
#include <fcntl.h>

int main()
{
  auto term = terminal::info::alloc();

  ::std::println("=== Test 1: H2 first, then H3, then H2 ===");
  ::std::println("Expected: 1, 1.1, 2\n");
  {
    // Redirect to file
    int fd = ::open("test1.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int saved = ::dup(STDOUT_FILENO);
    ::dup2(fd, STDOUT_FILENO);

    widget::textbox box{*term, "Test1"};
    box.add_markdown("## First H2\n### Nested H3\n## Second H2\n");
    box.draw();

    ::dup2(saved, STDOUT_FILENO);
    ::close(fd);
    ::close(saved);
  }

  ::std::println("\n=== Test 2: H3 first, then H4, then H3 ===");
  ::std::println("Expected: 1, 1.1, 2\n");
  {
    int fd = ::open("test2.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int saved = ::dup(STDOUT_FILENO);
    ::dup2(fd, STDOUT_FILENO);

    widget::textbox box{*term, "Test2"};
    box.add_markdown("### First H3\n#### Nested H4\n### Second H3\n");
    box.draw();

    ::dup2(saved, STDOUT_FILENO);
    ::close(fd);
    ::close(saved);
  }

  ::std::println("\n=== Test 3: H2, H3, then H1 appears (retroactive renumbering) ===");
  ::std::println("Expected after H1: 1.1, 1.1.1, 2, 2.1\n");
  {
    int fd = ::open("test3.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int saved = ::dup(STDOUT_FILENO);
    ::dup2(fd, STDOUT_FILENO);

    widget::textbox box{*term, "Test3"};
    box.add_markdown("## First H2\n### Nested H3\n# Top H1\n## Another H2\n");
    box.draw();

    ::dup2(saved, STDOUT_FILENO);
    ::close(fd);
    ::close(saved);
  }

  ::std::println("\n=== Test 4: Normal H1, H2, H3 ===");
  ::std::println("Expected: 1, 1.1, 1.1.1, 2\n");
  {
    int fd = ::open("test4.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int saved = ::dup(STDOUT_FILENO);
    ::dup2(fd, STDOUT_FILENO);

    widget::textbox box{*term, "Test4"};
    box.add_markdown("# First H1\n## Nested H2\n### Nested H3\n# Second H1\n");
    box.draw();

    ::dup2(saved, STDOUT_FILENO);
    ::close(fd);
    ::close(saved);
  }

  ::std::println("\nTest files created. Extracting numbering...\n");

  // Show results
  for (int i = 1; i <= 4; ++i) {
    ::std::print("Test {}: ", i);
    char cmd[100];
    ::snprintf(cmd, sizeof(cmd), "grep -oP '¶ \\K[0-9.]+' test%d.txt | tr '\\n' ' '", i);
    ::system(cmd);
    ::std::println();
  }

  return 0;
}
