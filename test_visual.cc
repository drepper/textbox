#include "textbox.hh"

int main()
{
  auto term = terminal::info::alloc();

  widget::textbox box{*term, "Heading Numbering Demo"};

  box.add_markdown(R"(
# Chapter One

This is the first chapter. H1 has no number.

## Introduction

This is a section under Chapter One. It shows "¶ 1".

### Background

This subsection shows "¶ 1.1".

### Motivation

This subsection shows "¶ 1.2".

## Methods

Second section under Chapter One. Shows "¶ 2".

# Chapter Two

Second chapter. Also has no number.

## Results

First section under Chapter Two. Shows "¶ 1".
)");

  box.draw();

  return 0;
}
