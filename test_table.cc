#include "textbox.hh"
#include "termdetect/termdetect.hh"

#include <unistd.h>

int main()
{
  auto term = ::terminal::info::alloc();

  widget::textbox tb{*term, "Table Test"};

  // Test simple table
  std::string markdown_table = R"(
# Table Example

Simple table with auto-bold headers:

| Name | Age | City |
|------|----:|:----:|
| Alice | 30 | NYC |
| Bob | 25 | LA |
| Charlie | 35 | Chicago |

Table with custom header formatting:

| *Feature* | `Description` | **Status** |
|:----------|:--------------|:----------:|
| Tables | Support for GitHub-style markdown tables | ✓ |
| Line breaking | Automatic line breaking at word boundaries | ✓ |
| Alignment | Left, center, and right alignment support | ✓ |

End of tables.
)";

  tb.add_markdown(markdown_table);

  ::sleep(5);

  return 0;
}
