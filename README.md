# C++ IO Stream Filters

## Introduction

C++ I/O Streams are designed on a two layer design.  The upper layer is what
most developers are familiar with and provides a fairly generic interface that
works the same way regardless of whether data is streamed to/from the console, a
file, or a string.  The lower layer, the streambuf, is what handles the actual
work of sending/receiving data to/from the underlying mechanism.

The obvious benefit of this design is that it allows developers the ability to
support other stream-able mechanisms like a TCP network connection or a UART
while providing the same usage semantics as other stream I/O.

An interesting trick is to change the underlying mechanism of an existing upper layer
stream object.  This makes it easy to do things like redirect console output to
a file without the need for macros or major search-and-replace operations.  It's
actually just as easy to select the underlying mechanism on the fly while the
rest of the code only needs to interact with a single interface; which macros
and search-and-replace can't do.

Another trick that can be done is to develop a streambuf that wraps another
streambuf.  The outer streambuf effectively becomes a filter that can transform
the data transferred to/from the inner streambuf.  That is what this project
provides, a collection of streambuf wrapper classes that act as filters.

## Filters

### Tabulator

This filter is for output streams.  It makes creating tables rather simple for
monospace output devices.  It supports left, center, and right justification as
well as word wrapping or truncation.  It has a basic understanding of UTF-8
characters such that multi-byte characters like the Euro (€) will still be
formatted properly.  (Characters that have a different cell size than the
ordinary characters found in the monospace font being used will adversely
affect the formatting of the table lines on the output device.)

It is possible to nest a table in a cell within another table.  See
[samples/tabulator_sample.cc](samples/tabulator_sample.cc) for details on all
the possible formatting options.

Here is a simple example of how to use the tabulator filter:
```
    using tab = ios_filter::tabulator;

    // Wrap the standard streambuf with the tabulator streambuf with 2 10 character wide columns
    auto filter = tab(std::cout, { 10, 10 });

    std::cout << ios_filter::set_style(tab::box); // Set the style to use UTF-8 box characters
    std::cout << ios_filter::top_line;            // Draw the top line of the table
    std::cout << "hello" << ios_filter::endc;     // Write text to the first cell
    std::cout << "world" << ios_filter::endc;     // Write text to the second cell
    std::cout << ios_filter::bottom_line;         // Draw the bottom line of the table
```
This is the output:
```
┌────────────┬────────────┐
│ hello      │ world      │
└────────────┴────────────┘
```

### Logging Formatter

This filter simplifies the code necessary for outputting the boilerplate
information in logging lines.  It even produces nice results for logging entries
that extend across multiple lines.  It is actually built on top of the tabulator filter


## Possible Future Filters

These are not implemented but represent ideas for future development.

### HTML Tabulator

(Not developed yet)
This is a variation of the Tabulator filter that will insert HTML tags necessary
for rendering the output in a web browser as a table.  This can be combined with
HTML Encode/Decode.

### HTML Encode/Decode

(Not developed yet)
This will translate special characters into HTML escape sequences like for the
greater than and less than symbols.  This can be combined with the HTML Tabulator.

### UTF-16 Byte Order Conversion

(Not developed yet)
For input streams, this will determine the data stream's
byte order from the BOM (Byte Order Marker) and byte-swap each character into
the native byte order if necessary.

For output streams, this will encode the BOM when created and write each
character in the specified byte order.

### UTF-32 Byte Order Conversion

(Not developed yet)
For input streams, this will determine the data stream's
byte order from the BOM (Byte Order Marker) and byte-swap each character into
the native byte order if necessary.

For output streams, this will encode the BOM when created and write each
character in the specified byte order.

### UTF-8, UTF-16, and UTF-32 Conversion

(Not developed yet)
This filter provides on-the-fly conversion between UFT-8, UTF-16, and UTF-32
while reading from or writing to a stream.  If the first character is the BOM
character, then automatic conversion is possible.

## Notes

1. The master branch will always remain stable.  Development branches, however,
   may experience changes in history (i.e., `git rebase -i`).

2. This project is licensed under the terms of the Apache 2.0 license.  See the
   [LICENSE](LICENSE) file for details.
