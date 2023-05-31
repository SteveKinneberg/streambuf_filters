/**
 * @file
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

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include "ios_filter/tabulator.h"

using namespace ios_filter;

bool operator== (tabulator::style_info::row const& lhs, tabulator::style_info::row const& rhs)
{
    return &lhs == &rhs || (lhs._left   == rhs._left   &&
                            lhs._center == rhs._center &&
                            lhs._right  == rhs._right  &&
                            lhs._line   == rhs._line);
}

bool operator== (tabulator::style_info const& lhs, tabulator::style_info const& rhs)
{
    return &lhs == &rhs || (lhs._top    == rhs._top    &&
                            lhs._middle == rhs._middle &&
                            lhs._bottom == rhs._bottom &&
                            lhs._cell   == rhs._cell);
}

std::ostream& operator<< (std::ostream& os, tabulator::style_info const& s)
{
    if (s == tabulator::empty)       return os << "style: empty";
    if (s == tabulator::ascii)       return os << "style: ascii";
    if (s == tabulator::markdown)    return os << "style: markdown";
    if (s == tabulator::box)         return os << "style: box";
    if (s == tabulator::rounded_box) return os << "style: rounded box";
    if (s == tabulator::heavy_box)   return os << "style: heavy box";
    if (s == tabulator::double_box)  return os << "style: double box";
    return os << "style: other";
}


class tabulator_test: public ::testing::Test {
  protected:
    std::ostringstream os;
    std::unique_ptr<tabulator> filter;

    void SetUp() override {
        os.str("");
        filter = std::make_unique<tabulator>(os, GetCells());
    }

    void TearDown() override {
        filter.reset();
    }

    virtual tabulator::cells_type GetCells() { return tabulator::cells_type{ 10 }; }
};


template <std::size_t N, std::size_t W = 10>
class tabulator_column_test: public tabulator_test {
  protected:
    void SetUp() override {
        tabulator_test::SetUp();
        filter->set_style(tabulator::ascii);
    }

    virtual tabulator::cells_type GetCells() {
        return tabulator::cells_type(N, tabulator::cell_info(W));
    }
};

using tabulator_1_column_test = tabulator_column_test<1, 10>;
TEST_F(tabulator_1_column_test, Style)
{
    os << set_style(tabulator::double_box);
    os << '\n' << endc;
    EXPECT_EQ(os.str(), "║            ║\n");
}


TEST_F(tabulator_1_column_test, padding)
{
    os << set_pad("A", "B") << '\n' << endc;
    EXPECT_EQ(os.str(), "|A          B|\n");
}

TEST_F(tabulator_1_column_test, wrapping_ascii_character)
{
    os << set_wrap(wrap::character) << "abcdef ghijkl" << endc;
    EXPECT_EQ(os.str(),
              "| abcdef ghi |\n"
              "| jkl        |\n");
}

TEST_F(tabulator_1_column_test, wrapping_ascii_word)
{
    os << set_wrap(wrap::word) << "abcdef ghijkl" << endc;
    EXPECT_EQ(os.str(),
              "| abcdef     |\n"
              "| ghijkl     |\n");
}

TEST_F(tabulator_1_column_test, wrapping_utf8_character)
{
    os << set_wrap(wrap::character) << R"(ăƀçđêƒ ǧĥïĵǩĺ)" << endc;
    EXPECT_EQ(os.str(),
              R"(| ăƀçđêƒ ǧĥï |)" "\n"
              R"(| ĵǩĺ        |)" "\n");
}

TEST_F(tabulator_1_column_test, wrapping_utf8_word)
{
    os << set_wrap(wrap::word) << R"(ăƀçđêƒ ǧĥïĵǩĺ)" << endc;
    EXPECT_EQ(os.str(),
              R"(| ăƀçđêƒ     |)" "\n"
              R"(| ǧĥïĵǩĺ     |)" "\n");
}

TEST_F(tabulator_1_column_test, wrapping_at_whitespace_character_1)
{
    os << set_wrap(wrap::character) << "abcdefghi jklmno" << endc;
    EXPECT_EQ(os.str(),
              "| abcdefghi  |\n"
              "| jklmno     |\n");
}

TEST_F(tabulator_1_column_test, wrapping_at_whitespace_character_2)
{
    os << set_wrap(wrap::character) << "abcdefghij klmno" << endc;
    EXPECT_EQ(os.str(),
              "| abcdefghij |\n"
              "| klmno      |\n");
}

TEST_F(tabulator_1_column_test, wrapping_at_whitespace_character_3)
{
    os << set_wrap(wrap::character) << "abcdefghijk lmno" << endc;
    EXPECT_EQ(os.str(),
              "| abcdefghij |\n"
              "| k lmno     |\n");
}

TEST_F(tabulator_1_column_test, wrapping_at_whitespace_word_1)
{
    os << set_wrap(wrap::word) << "abcdefghi jklmno pqrstuv" << endc;
    EXPECT_EQ(os.str(),
              "| abcdefghi  |\n"
              "| jklmno     |\n"
              "| pqrstuv    |\n");
}

TEST_F(tabulator_1_column_test, wrapping_at_whitespace_word_2)
{
    os << set_wrap(wrap::word) << "abcdefghij klmno pqrstuv" << endc;
    EXPECT_EQ(os.str(),
              "| abcdefghij |\n"
              "| klmno      |\n"
              "| pqrstuv    |\n");
}

TEST_F(tabulator_1_column_test, wrapping_at_whitespace_word_3)
{
    os << set_wrap(wrap::word) << "abcdefghijk lmno pqrstuv" << endc;
    EXPECT_EQ(os.str(),
              "| abcdefghij |\n"
              "| k lmno     |\n"
              "| pqrstuv    |\n");
}

TEST_F(tabulator_1_column_test, truncate_left_one_line_short_character)
{
    os << set_truncate(truncate::left) << "abc" << endc;
    EXPECT_EQ(os.str(), "| abc        |\n");
}

TEST_F(tabulator_1_column_test, truncate_left_one_line_character)
{
    os << set_truncate(truncate::left) << "abcdef ghijkl" << endc;
    EXPECT_EQ(os.str(), "| …ef ghijkl |\n");
}

TEST_F(tabulator_1_column_test, truncate_right_one_line_short_character)
{
    os << set_truncate(truncate::right) << "abc" << endc;
    EXPECT_EQ(os.str(), "| abc        |\n");
}

TEST_F(tabulator_1_column_test, truncate_right_one_line_character)
{
    os << set_truncate(truncate::right) << "abcdef ghijkl" << endc;
    EXPECT_EQ(os.str(), "| abcdef gh… |\n");
}

TEST_F(tabulator_1_column_test, truncate_left_one_line_word)
{
    os << set_wrap(wrap::word) << set_truncate(truncate::left)  << "abcdef ghijkl" << endc;
    EXPECT_EQ(os.str(), "| …ghijkl    |\n");
}

TEST_F(tabulator_1_column_test, truncate_right_one_line_word)
{
    os << set_wrap(wrap::word) << set_truncate(truncate::right) << "abcdef ghijkl" << endc;
    EXPECT_EQ(os.str(), "| abcdef…    |\n");
}

TEST_F(tabulator_1_column_test, truncate_left_multiline_character)
{
    os << set_truncate(truncate::left)  << "123456 ghijkl\nmnopqr stuvwx" << endc;
    EXPECT_EQ(os.str(), "| …qr stuvwx |\n");
}

TEST_F(tabulator_1_column_test, truncate_right_multiline_character)
{
    os << set_truncate(truncate::right) << "123456 ghijkl\nmnopqr stuvwx" << endc;
    EXPECT_EQ(os.str(), "| 123456 gh… |\n");
}

TEST_F(tabulator_1_column_test, truncate_left_multiline_word)
{
    os << set_wrap(wrap::word) << set_truncate(truncate::left)  << "123456 ghijkl\nmnopqr stuvwx" << endc;
    EXPECT_EQ(os.str(), "| …stuvwx    |\n");
}

TEST_F(tabulator_1_column_test, truncate_right_multiline_word)
{
    os << set_wrap(wrap::word) << set_truncate(truncate::right) << "123456 ghijkl\nmnopqr stuvwx" << endc;
    EXPECT_EQ(os.str(), "| 123456…    |\n");
}

using tabulator_2_column_test = tabulator_column_test<2, 10>;
TEST_F(tabulator_2_column_test, short_line_1)
{
    os << '\n' << endc << endc;
    EXPECT_EQ(os.str(), "|            |            |\n");
}

TEST_F(tabulator_2_column_test, short_line_2)
{
    os << "abc" << endc << endc;
    EXPECT_EQ(os.str(), "| abc        |            |\n");
}

TEST_F(tabulator_2_column_test, short_line_3)
{
    os << "abc" << endc << "123" << endc;
    EXPECT_EQ(os.str(), "| abc        | 123        |\n");
}

TEST_F(tabulator_2_column_test, zero_width_1)
{
    os << set_width(0);
    os << '\n' << endc;
    os << set_width(0);
    os << endc;
    EXPECT_EQ(os.str(), "|  |  |\n");
}

TEST_F(tabulator_2_column_test, zero_width_2)
{
    os << set_width(0);
    os << "hello world" << endc;
    os << set_width(0);
    os << endc;
    EXPECT_EQ(os.str(), "| hello world |  |\n");
}

TEST_F(tabulator_2_column_test, zero_width_3)
{
    os << set_width(0);
    os << endc;
    os << set_width(0);
    os << "hello world" << endc;
    EXPECT_EQ(os.str(), "|  | hello world |\n");
}

TEST_F(tabulator_2_column_test, zero_width_4)
{
    os << set_width(0);
    os << "hello" << endc;
    os << set_width(0);
    os << "world" << endc;
    EXPECT_EQ(os.str(), "| hello | world |\n");
}


using tabulator_3_column_test = tabulator_column_test<3, 10>;
TEST_F(tabulator_3_column_test, justify)
{
    os << set_justify(justify::right) << "1234" << endc
       << set_justify(justify::center) << "1234" << endc
       << set_justify(justify::left) << "1234" << endc;
    EXPECT_EQ(os.str(),
              "|       1234 |    1234    | 1234       |\n");
}


using tabulator_nested_test = tabulator_column_test<2, 20>;
TEST_F(tabulator_nested_test, nested_1)
{
    os << "one" << endc;
    {
        auto filter2 = tabulator(os, { 5, 5 });
        filter2.set_style(tabulator::ascii);
        os << "12345678" << endc << "abcd" << endc;
    }
    os << endc;
    EXPECT_EQ(os.str(),
              "| one                  | | 12345 | abcd  |    |\n"
              "|                      | | 678   |       |    |\n");
}

TEST_F(tabulator_nested_test, nested_2)
{
    os << "one" << endc;
    {
        auto filter2 = tabulator(os, { 5, 5 });
        filter2.set_style(tabulator::ascii);
        os << "12345678" << endc << endc;
    }
    os << endc;
    EXPECT_EQ(os.str(),
              "| one                  | | 12345 |       |    |\n"
              "|                      | | 678   |       |    |\n");
}



using expected_strings = std::array<std::string_view, 4>;
using test_tuple = std::tuple<tabulator::style_info, expected_strings>;

class tabulator_style_test: public tabulator_test, public ::testing::WithParamInterface<test_tuple> {
  protected:
    expected_strings expected;

    void SetUp() override {
        tabulator_test::SetUp();
        tabulator::style_info style;
        std::tie(style, expected) = GetParam();
        filter->set_style(style);
    }

    void TearDown() override {
        tabulator_test::TearDown();
    }

    tabulator::cells_type GetCells() override { return tabulator::cells_type{ 0, 0 }; }
};


TEST_P(tabulator_style_test, top_line)
{
    os << top_line;
    EXPECT_EQ(os.str(), expected[0]);
}

TEST_P(tabulator_style_test, horiz_line)
{
    os << horiz_line;
    EXPECT_EQ(os.str(), expected[1]);
}

TEST_P(tabulator_style_test, bottom_line)
{
    os << bottom_line;
    EXPECT_EQ(os.str(), expected[2]);
}

TEST_P(tabulator_style_test, empty_cells)
{
    os << '\n' << endc << endc << endc;
    EXPECT_EQ(os.str(), expected[3]);
}

INSTANTIATE_TEST_CASE_P(
    styles,
    tabulator_style_test,
    ::testing::Values(
         test_tuple(tabulator::empty,
                   { "\n",
                     "\n",
                     "\n",
                     "    \n" }),
         test_tuple(tabulator::ascii,
                   { "+--+--+\n",
                     "+--+--+\n",
                     "+--+--+\n",
                     "|  |  |\n" }),
         test_tuple(tabulator::markdown,
                   { "\n",
                     "--|--\n",
                     "\n",
                     "  |  \n" }),
         test_tuple(tabulator::box,
                   { R"(┌──┬──┐)" "\n",
                     R"(├──┼──┤)" "\n",
                     R"(└──┴──┘)" "\n",
                     R"(│  │  │)" "\n"}),
         test_tuple(tabulator::double_box,
                   { R"(╔══╦══╗)" "\n",
                     R"(╠══╬══╣)" "\n",
                     R"(╚══╩══╝)" "\n",
                     R"(║  ║  ║)" "\n" }),
         test_tuple(tabulator::heavy_box,
                   { R"(┏━━┳━━┓)" "\n",
                     R"(┣━━╋━━┫)" "\n",
                     R"(┗━━┻━━┛)" "\n",
                     R"(┃  ┃  ┃)" "\n" }),
         test_tuple(tabulator::rounded_box,
                   { R"(╭──┬──╮)" "\n",
                     R"(├──┼──┤)" "\n",
                     R"(╰──┴──╯)" "\n",
                     R"(│  │  │)" "\n"}),
         test_tuple(tabulator::style_info {
                 { "t<", "t|", "t>" , "t-" },
                 { "m<", "m|", "m>" , "m-" },
                 { "b<", "b|", "b>" , "b-" },
                 { "c<", "c|", "c>" , ""   }
             }, { R"(t<t-t|t-t>)" "\n",
                  R"(m<m-m|m-m>)" "\n",
                  R"(b<b-b|b-b>)" "\n",
                  R"(c<  c|  c>)" "\n" } ) ));
