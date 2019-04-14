/**
 * @file ""
 *
 * Unit test file for testing the tabulator streambuf filter.
 *
 * Copyright 2019 Steve Kinneberg <steve.kinneberg@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <chrono>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <thread>
#include "ios_filter/tabulator.h"

using namespace std;
using namespace ios_filter;

#define EXAMPLE_CODE(name, desc, code)                                  \
    void _##name()                                                      \
    {                                                                   \
        code;                                                           \
    }                                                                   \
    void name()                                                         \
    {                                                                   \
        cout << horiz_line;                                             \
        cout << set_wrap(wrap::word) << desc << endc << endc            \
             << set_wrap(wrap::character) << "\n" << format_code(#code) << endc; \
        _##name();                                                      \
        cout << endc << flush;                                          \
    }

string format_code(string_view code)
{
    return regex_replace(code.data(), regex("; "), ";\n");
}

EXAMPLE_CODE(basic,
             "Basic usage with default options for 2 10 character columns",
             using tab = tabulator;
             auto filter = tab(cout, { 10, 10 });
             cout << top_line;
             cout << "hello" << endc;
             cout << "world" << endc;
             cout << bottom_line;
             )

EXAMPLE_CODE(empty_style,
             "No lines between colums (empty style)",
             using tab = tabulator;
             auto filter = tab(cout, { 10, 10 });
             cout << set_style(tab::empty);
             cout << top_line;
             cout << "hello" << endc;
             cout << "world" << endc;
             cout << bottom_line;
             )

EXAMPLE_CODE(ascii_style,
             "Using ASCII characters for line drawing (ascii style)",
             using tab = tabulator;
             auto filter = tab(cout, { 10, 10 });
             cout << set_style(tab::ascii);
             cout << top_line;
             cout << "hello" << endc;
             cout << "world" << endc;
             cout << bottom_line;
             )

EXAMPLE_CODE(box_style,
             "Using UTF8 line characters (box style)",
             using tab = tabulator;
             auto filter = tab(cout, { 10, 10 });
             cout << set_style(tab::box);
             cout << top_line;
             cout << "hello" << endc;
             cout << "world" << endc;
             cout << bottom_line;
             )

EXAMPLE_CODE(double_box_style,
             "Using UTF8 double line characters (double_box style)",
             using tab = tabulator;
             auto filter = tab(cout, { 10, 10 });
             cout << set_style(tab::double_box);
             cout << top_line;
             cout << "hello" << endc;
             cout << "world" << endc;
             cout << bottom_line;
             )

EXAMPLE_CODE(heavy_box_style,
             "Using UTF8 heavy line characters (heavy_box style)",
             using tab = tabulator;
             auto filter = tab(cout, { 10, 10 });
             cout << set_style(tab::heavy_box);
             cout << top_line;
             cout << "hello" << endc;
             cout << "world" << endc;
             cout << bottom_line;
             )

EXAMPLE_CODE(rounded_box_style,
             "Using UTF8 rounder corner characters (rounded_box style)",
             using tab = tabulator;
             auto filter = tab(cout, { 10, 10 });
             cout << set_style(tab::rounded_box);
             cout << top_line;
             cout << "hello" << endc;
             cout << "world" << endc;
             cout << bottom_line;
             )

EXAMPLE_CODE(justification,
             "Text position within a cell",
             using tab = tabulator;
             auto filter = tab(cout, { 25 });
             cout << set_style(tab::box);
             cout << top_line;
             cout << set_justify(justify::left);
             cout << "left"  << endc;
             cout << horiz_line;
             cout << set_justify(justify::center);
             cout << "center" << endc;
             cout << horiz_line;
             cout << set_justify(justify::right);
             cout << "right"  << endc;
             cout << bottom_line;
             )

EXAMPLE_CODE(wrapping,
             "Text wrapping in a cell",
             using tab = tabulator;
             auto filter = tab(cout, { 25 });
             cout << set_style(tab::box);
             cout << top_line;
             cout << set_wrap(wrap::character);
             cout << "This is an example of character wrapping" << endc;
             cout << horiz_line;
             cout << set_wrap(wrap::word);
             cout << "This is an example of word wrapping" << endc;
             cout << bottom_line;
             )


EXAMPLE_CODE(truncation,
             "Text wrapping in a cell",
             using tab = tabulator;
             auto filter = tab(cout, { 25 });
             cout << set_style(tab::box);
             cout << top_line;
             cout << set_truncate(truncate::right);
             cout << "This is an example of truncation" << endc;
             cout << horiz_line;
             cout << set_truncate(truncate::left);
             cout << "This is an example of truncation" << endc;
             cout << bottom_line;
             )

EXAMPLE_CODE(multibyte_character,
             "Formating with multi-byte UTF characters",
             using tab = tabulator;
             auto filter = tab(cout, { 12, 12 });
             cout << set_style(tab::box);
             cout << top_line;
             cout << set_wrap(wrap::word);
             cout << "English" << endc;
             cout << set_wrap(wrap::word);
             cout << "Ελληνικά" << endc;
             cout << horiz_line;
             cout << "Hello World." << endc;
             cout << "Γειά σου Κόσμε." << endc;
             cout << bottom_line;
             )

EXAMPLE_CODE(flush_mid_table,
             "Flush mid table",
             using namespace chrono_literals;
             using tab = tabulator;
             auto filter = tab(cout, { 10, 10 });
             cout << set_style(tab::box);
             cout << top_line;
             cout << "Wait 3s" << endc;
             cout << "3..." << endl;
             this_thread::sleep_for(1s);
             cout << "2..." << endl;
             this_thread::sleep_for(1s);
             cout << "1..." << endl;
             this_thread::sleep_for(1s);
             cout << "DONE" << endc;
             cout << bottom_line;
             )


void print_example_table()
{
    auto filter = tabulator(cout, { 45, 32 });
    cout << set_style(tabulator::borderless_box);

    cout << set_pad("", " ") << set_wrap(wrap::word) << "Description and example code" << endc
         << set_pad(" ", "")  << "Output" << endc;

    basic();
    empty_style();
    ascii_style();
    box_style();
    double_box_style();
    heavy_box_style();
    rounded_box_style();
    justification();
    wrapping();
    truncation();
    multibyte_character();
    flush_mid_table();
}

int main()
{
    print_example_table();

    return 0;
}
