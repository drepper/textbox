# Table Support in Textbox Widget

The textbox widget now supports GitHub-style markdown tables with the following features:

## Features

1. **Column Alignment**: Support for left (`:---`), center (`:---:`), and right (`---:`) alignment
2. **Automatic Line Breaking**: Text in cells automatically wraps at word boundaries if columns are too wide
3. **Dynamic Width Calculation**: Tables automatically size columns based on content
4. **Overflow Handling**: If a table is too wide for the terminal, it shows only the columns that fit
5. **Unicode Box Drawing**: Uses Unicode box drawing characters (U+2500 block) for clean table borders
6. **Bold Headers**: Table headers are automatically rendered in bold unless they already contain markup

## Syntax

Tables follow GitHub markdown syntax:

```markdown
| Header 1 | Header 2 | Header 3 |
|----------|:--------:|---------:|
| Left     | Center   | Right    |
| Content  | More     | Data     |
```

- First row: Table headers
- Second row: Separator with alignment indicators
  - `---` or `:---` = left aligned (default)
  - `:---:` = center aligned
  - `---:` = right aligned
- Remaining rows: Table data

## Example

```cpp
widget::textbox tb{term, "My Widget"};

tb.add_markdown(R"(
| Name | Age | City |
|------|----:|:----:|
| Alice | 30 | NYC |
| Bob | 25 | LA |
| Charlie | 35 | Chicago |
)");
```

This renders as:

```
┌────────────────────────────────────┐
│ Name    │  Age │      City      │  ← Headers automatically bold
├─────────┼──────┼────────────────┤
│ Alice   │   30 │      NYC       │
│ Bob     │   25 │       LA       │
│ Charlie │   35 │    Chicago     │
└────────────────────────────────────┘
```

## Header Formatting

Table headers are automatically rendered in **bold** unless the header text already contains inline formatting markup (`**`, `*`, `_`, `` ` ``, or `~~`).

Examples:

```markdown
| Name | Age |           ← Both headers will be bold
|------|-----|

| *Name* | **Age** |   ← Custom formatting preserved (italic and bold)
|--------|---------|

| `Code` | Status |    ← Code formatting and auto-bold
|--------|--------|
```

## Implementation Details

- Tables are stored as a vector of rows (each row is a vector of strings)
- Column alignment information is stored separately
- During rendering:
  1. Calculate minimum width for each column
  2. If total width exceeds available space, distribute width proportionally
  3. Wrap cell content at word boundaries
  4. Apply alignment when padding cells
  5. Use box drawing characters: `│` `─` `┌` `┐` `└` `┘` `├` `┤` `┬` `┴` `┼`
