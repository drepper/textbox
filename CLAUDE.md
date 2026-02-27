This project is a C++ class for a text widget rendered in a terminal emulator using available escape sequences for placement,
movement, color etc.  assume the output encoding is UTF-8.


Use the termdetect library from the termdetect subdirectory to retrieve information about the terminal.
The constructure for the class should expect a reference to a terminal::info object and use it (the lifetime
of that object is longer than that of the textbox widget, a reference can be store).

the constructor for the object of the widget gets passed string which is the name of the widget. That name is
also used as the default title of the widget when it is displayed.  There title can be changed with a member
function set_title which when called also triggers rewriting the title on the screen.

The widget displays a series of strings which are displayed as paragraphs with an empty line in between.  Store the
paragraph data as a vector of paragraph objects which contains string and other information like flags.

The content is displayed over the width of the terminal with a margin which individually can be specified for the
right and left.  Both are zero unless overwritten.  Around all the text content optionally a frame is drawn.  The options
for the frame are:
- 'none': no frame
- 'line': use ╭ ─ ╮ for the top, use │ for the right and left side, use ╰ ─ ╯ for the bottom.
- 'background': use ▗ ▄ ▖ for the top use ▐ for the left side, use ▌ for the right side, ▝ ▀ ▘ for the bottom

the default is line.

use separately selectable colors for the frame and the content.  The frame only needs a foreground color and always
should use the default background.  The same is true for the text background unless 'background' is used for the
frame.  In that case the selected background is used.  It is
initialized to a very dark red, the text foreground to 127/127/127,
and the frame foreground to a 30% brighter red than the text background.

when the object for the widget is create get the current screen position. Assume for the remainder of the lifetime
of the widget that no screen scrolling happens outside of the control of the widget.  It is always possible to
redraw the entire widget or parts after moving the cursor according to the recorded coordinates.

content is added to the widget using two member functions:
- add_text which takes a string parameter.   The content of the string is parsed for newlines.  Characters are
  appended to the current paragraph. when a newline character is found the current paragraph is closed and a
  new started.  No paragrph is empty.  In that case the newline character is ignored.  Also ignore all carriage
  return characters.  A paragraph created this way marked as 'reflow'.
- add_block which takes a vector of strings parameter.  A new paragraph is created with the appended content
  of the strings in the vector.  if the strings do not end with a newline, append one.  Mark the paragraph as
  'fixed'.  If before the call an empty paragraph was in the list, insert this fixed paragraph before it.
  Otherwise close the previous paragraph.  Ensure that after this new paragraph is inserted there always is a
  new, empty paragraph in the list.

Adding text causes the wdiget to be re-rendered.

The content is rendered according to the width of the inside of the frame.  Characters are encoded using UTF-8,
can be multi-column, and there can be escape sequences (CSI, OSC, etc).  The computation of the width of the output
has to be computed with this in mind.  Paragraphs marked with 'reflow' are automatically wrapped if the content
is longer than what fits into a single line and the paragraph can take up multiple lines.  Long lines in 'fixed' paragraphs
are truncated to fit into the frame.

The title of the widget is shown in the top line of the frame, replacing the initial horizontal characters.  If the title
is too long an would overwrite the right end character, it is truncated.  Multi-column characters and escape sequences
are allowed in the string.
