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

Here is a simple table:

| Name | Age | City |
|------|----:|:----:|
| Alice | 30 | NYC |
| Bob | 25 | LA |
| Charlie | 35 | Chicago |

And here's a table with longer content:

| Feature | Description | Status |
|:--------|:------------|:------:|
| Tables | Support for GitHub-style markdown tables | ✓ |
| Line breaking | Automatic line breaking at word boundaries | ✓ |
| Alignment | Left, center, and right alignment support | ✓ |

End of tables.
)";

  tb.add_markdown(markdown_table);

  ::sleep(5);

  return 0;
}
