#include "textbox.hh"
#include "termdetect/termdetect.hh"

#include <unistd.h>

int main()
{
  auto term = ::terminal::info::alloc();

  widget::textbox tb{*term, "Unified Diff Test"};

  // Sample unified diff
  std::string diff = R"(--- a/example.cpp
+++ b/example.cpp
@@ -10,7 +10,8 @@ int calculate(int x, int y)
 {
   if (x < 0)
     return -1;
-  return x * y;
+  // Fixed: handle overflow
+  return x + y;
 }

 int main()
@@ -20,6 +21,7 @@ int main()
   int result = calculate(a, b);
   std::cout << "Result: " << result << std::endl;
+  std::cout << "Done!" << std::endl;
   return 0;
 }
)";

  tb.add_udiff(diff);

  ::sleep(5);

  return 0;
}
