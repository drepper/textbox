#include "textbox.hh"

int main()
{
  auto term = terminal::info::alloc();

  widget::textbox box{*term, "Manual Numbering Test"};

  box.add_markdown(R"(
# 1. First Chapter

This chapter has "1." in the markdown.

## 1.1 First Section

This section has "1.1" in the markdown.

## 1.2 Second Section

This section has "1.2" in the markdown.

# 2. Second Chapter

## Introduction

This one has no manual number.

## 3.1.4 Weird Section

This has a complex number that should be stripped.
)");

  box.draw();

  return 0;
}
