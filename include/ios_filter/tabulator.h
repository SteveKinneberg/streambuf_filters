#ifndef _ios_filter_tabulator_h
#define _ios_filter_tabulator_h
/**
 * @file
 *
 * Tabulator IO stream filter for output streams.
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

#if __cplusplus < 201703L
#error IO stream filters require C++17 or later.
#endif

#include <algorithm>
#include <cassert>
#include <deque>
#include <functional>
#include <iterator>
#include <streambuf>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include "utf_utils.h"

#include <iostream>
#include <iomanip>

/// @cond IGNORE
#if 1
#include <cxxabi.h>
#include <type_traits>
#include <typeinfo>
namespace std {

inline auto to_string(const std::type_info& info) noexcept
{
    std::string info_str;

    char *realname = abi::__cxa_demangle(info.name(), nullptr, nullptr, nullptr);
    if (realname) {
        info_str = realname;
        free(realname);
    }

    return info_str;
}
}

inline auto& operator<<(std::ostream& os, const std::type_info& typeInfo) noexcept
{
    return os << std::to_string(typeInfo);
}

#define LOG(x) std::cerr << std::boolalpha << "!\t\t" << std::setw(4) << __LINE__ << " SJK: " << x << std::endl
#else
#define LOG(x)
#endif
/// #endcond

namespace ios_filter {
/**
 * Table cell justification.
 *
 * This controls the justification of text in a table cell for a column of
 * cells.  This is only useful for table columns with a fixed width.
 */
enum class justify {
    left,       ///< Left justification.
    right,      ///< Center justification.
    center      ///< Right justification.
};

/**
 * Table cell truncation.
 *
 * This controls truncation of text in a table cell for a column of cells.  This
 * is only useful for table columns with a fixed width.
 */
enum class truncate {
    none,       ///< No truncation, text will be wrapped.
    left,       ///< Remove the left part of the text if it is too long to fit on one line.
    right       ///< Remove the right part of the text if it is too long to fit on one line.
};

/**
 * Table cell line wrapping.
 *
 * This controls if long lines of text are character or word wrapped.  This
 * is only useful for table columns with a fixed width that are not truncated.
 */
enum class wrap {
    character,  ///< Character wrapping of the text if it is too long to fit on one line.
    word        ///< Word wrapping of the text if it is too long to fit on one line.
};

/**
 * This class enables formatting the text for an output stream into a table.
 *
 * It works by wrapping the output stream's original streambuf and installing
 * itself as the output stream's streambuf.  Thus all calls to `operator <<` on
 * that output stream will be placed into one of the defined columns.
 *
 * This follows the RAII principle in that so long as the instance is in scope,
 * all output will be put into the table.  When the instance is destroyed, the
 * original streambuf will be restored to the output stream and table formatting
 * will end.
 *
 * It is possible to nest a table within a table.  Care must be taken when
 * specifying the column widths when doing so.
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 */
template <typename CharT, typename Traits = ::std::char_traits<CharT>>
class basic_tabulator: public ::std::basic_streambuf<CharT, Traits> {
    using base_type = ::std::basic_streambuf<CharT, Traits>;

  public:
    class cell_info;
    /// The character type.
    using char_type        = CharT;
    /// The character traits type.
    using traits_type      = Traits;
    /// The integer type used in streambufs.
    using int_type         = typename Traits::int_type;
    /// The position type used in streambufs.
    using pos_type         = typename Traits::pos_type;
    /// The offset type used in streambufs.
    using off_type         = typename Traits::off_type;
    /// The output stream type.
    using ostream_type     = ::std::basic_ostream<CharT, Traits>;
    /// The sting type that corresponds to char_type.
    using string_type      = ::std::basic_string<CharT, Traits>;
    /// The sting_view type that corresponds to char_type.
    using string_view_type = ::std::basic_string_view<CharT, Traits>;
    /// The container type for the list of cells.
    using cells_type =       ::std::vector<cell_info>;

    /**
     * This is responsible for rendering each line of text in a table cell.
     */
    class cell_info {
#define SLI(x) basic_tabulator<CharT, Traits>::string_literal_init(x, L##x, u##x, U##x)
      public:
        /// The basic strembuf base class type.
        using rdbuf_type = std::basic_streambuf<CharT, Traits>;
        /// The type for the padding tuple.
        using pad_type   = std::tuple<string_type, string_type>;

        /// ASCII representation of an ellipsis.
        static constexpr string_view_type ASCII_ELLIPSIS = SLI("...");

        /// UTF8 representation of single character ellipsis (…).
        static constexpr string_view_type UTF8_ELLIPSIS = SLI("\u2026");

        /**
         * Constructor.
         *
         * @param width     Width of the cell.
         * @param lpad      Left padding (does not count against the width).
         * @param rpad      Right padding (does not count against the width).
         * @param ellipsis  Ellipsis string when truncating text.
         */
        cell_info(std::size_t width = 0,
                  string_view_type lpad = " ", string_view_type rpad = " ",
                  string_view_type ellipsis = UTF8_ELLIPSIS):
            _width(width),
            _written(0),
            _cell_start(true),
            _justify(justify::left), _truncate(truncate::none), _wrap(wrap::character),
            _pad(std::make_tuple(lpad, rpad)),
            _ellipsis(std::move(ellipsis))
        {
            assert(_width == 0 || _width > utflen(_ellipsis));
        }

        /**
         * Move constructor - default implementation.
         *
         * @param other     Other instance to move from.
         */
        cell_info(cell_info&& other) = default;

        /**
         * Copy constructor - default implementation.
         *
         * @param other     Other instance to copy from.
         */
        cell_info(cell_info const& other) = default;

        /**
         * Move constructor - default implementation.
         *
         * @param other     Other instance to move from.
         *
         * @return          A reference to *this.
         */
        cell_info& operator =(cell_info&& other) = default;

        /**
         * Copy constructor - default implementation.
         *
         * @param other     Other instance to copy from.
         *
         * @return          A reference to *this.
         */
        cell_info& operator =(cell_info const& other) = default;

        /**
         * Set the cell width.
         *
         * @param w     Width of the cell.
         *
         * @return      Reference to `*this` to enable set_*() function chaining.
         */
        auto& set_width(std::size_t w) noexcept {
            assert(_width == 0 || _width >= utflen(_ellipsis));
            _width = w;
            return *this;
        }

        /**
         * Set the cell justification.
         *
         * @param j     Justification of the cell.
         *
         * @return      Reference to `*this` to enable `set_*()` function chaining.
         */
        auto& set_justify(justify j) noexcept { _justify = j; return *this; }

        /**
         * Set the cell truncation.
         *
         * @param t     Truncation of the cell.
         *
         * @return      Reference to `*this` to enable `set_*()` function chaining.
         */
        auto& set_truncate(truncate t) noexcept {
            assert(_width == 0 || _width > utflen(_ellipsis));
            _truncate = t;
            return *this;
        }

        /**
         * Set the cell line wrapping.
         *
         * @param w     Line wrapping of the cell.
         *
         * @return      Reference to `*this` to enable `set_*()` function chaining.
         */
        auto& set_wrap(wrap w) noexcept { _wrap = w; return *this; }

        /**
         * Set the truncation ellipsis "character".
         *
         * @param e     Truncation ellipsis "character" of the cell.
         *
         * @return      Reference to `*this` to enable `set_*()` function chaining.
         */
        auto& set_ellipsis(string_type&& e) noexcept {
            assert(_width == 0 || _width > utflen(_ellipsis));
            _ellipsis = std::move(e);
            return *this;
        }

        /**
         * Set the truncation ellipsis "character".
         *
         * @param e     Truncation ellipsis "character" of the cell.
         *
         * @return      Reference to `*this` to enable `set_*()` function chaining.
         */
        auto& set_ellipsis(string_view_type e) noexcept {
            assert(_width == 0 || _width > utflen(_ellipsis));
            _ellipsis = e;
            return *this;
        }

        /**
         * Set the left and right cell padding.
         *
         * @param p     Tuple of the left (first) and right (second) cell padding.
         *
         * @return      Reference to `*this` to enable `set_*()` function chaining.
         */
        auto& set_pad(pad_type&& p) noexcept { _pad = std::move(p); return *this; }

        /**
         * Set the left and right cell padding.
         *
         * @param p     Tuple of the left (first) and right (second) cell padding.
         *
         * @return      Reference to `*this` to enable `set_*()` function chaining.
         */
        auto& set_pad(pad_type const& p) noexcept { _pad = p; return *this; }

        /**
         * Get the currently set left and right pad strings.
         *
         * @return      A tuple of the left (first), and right (second) padding strings.
         */
        auto const& get_pad() const noexcept { return _pad; }

        /**
         * Get the current column width.
         *
         * @return      The current column width.
         */
        std::size_t get_width() const noexcept { return _width; }

        /**
         * Get the current cell width.  This includes the left and right padding.
         *
         * @return      The current cell width.
         */
        std::size_t get_cell_width() const noexcept
        {
            auto const& [lpad, rpad] = _pad;
            return _width + utflen(lpad) + utflen(rpad);
        }

        /**
         * Put a character in the end of the column buffer.
         *
         * @param ch    The character to put in the end of the buffer.
         */
        void putc(char_type ch) { _buf.push_back(ch); }

        /**
         * Write a line of characters to the given streambuf and remove those
         * characters from the buffer.
         *
         * @param sbuf              The streambuf to send characters to.
         * @param write_full_line   Flag indicating if a full line should be written.
         *
         * @return      Whether or not there are more characters left in the buffer:
         *              - `true`:  There are more characters in the buffer.
         *              - `false`: There are no more characters in the buffer.
         */
        bool write_line(rdbuf_type& sbuf, bool write_full_line)
        {
            auto wrote_full = write_full_line;
            if (write_full_line || _truncate == truncate::none) {
                auto end = truncate_buf();
                std::size_t out_width = utflen(_buf.cbegin(), end) + _written;
                std::size_t lfill = 0;
                std::size_t rfill = 0;

                if (!write_full_line && out_width < _width && _justify != justify::left) {
                    return wrote_full;
                }

                if (_width > 0) {
                    int sum = 0;
                    while (out_width > _width)
                    {
                        sum += utf_char_score(*end);
                        --end;
                        if (sum == 0) { --out_width; }
                    }
                    wrote_full = wrote_full || (out_width == _width);

                    auto total_fill = _width - out_width;
                    switch (_justify) {
                    case justify::center:
                        lfill = (total_fill) / 2;
                        rfill = total_fill - lfill;
                        break;
                    case justify::left:   rfill = total_fill; break;
                    case justify::right:  lfill = total_fill; break;
                    }
                }

                auto const& [lpad, rpad] = _pad;

                if (_cell_start) {
                    sbuf.sputn(lpad.data(), lpad.size());
                    draw_fill(sbuf, lfill);
                    _cell_start = false;
                }

                while (_buf.begin() != end) {
                    sbuf.sputc(_buf.front());
                    _buf.pop_front();
                }
                wrote_full = wrote_full || (_buf.front() == '\n');

                if (!empty() && std::isspace(_buf.front())) { _buf.pop_front(); }
                if (_wrap == wrap::word) {
                    while (!empty() && std::isspace(_buf.front())) { _buf.pop_front(); }
                }
                wrote_full = wrote_full || !_buf.empty();

                if (wrote_full) {
                    draw_fill(sbuf, rfill);
                    sbuf.sputn(rpad.data(), rpad.size());
                    _cell_start = true;
                    _written = 0;
                } else {
                    _written = out_width;
                }
            }
            return wrote_full;
        }

        /**
         * Check if the cell buffer is empty.
         *
         * @return      Indicates if the cell buffer is empty:
         *              - `true`: buffer is empty.
         *              - `false`: buffer is not empty.
         */
        bool empty() const { return _buf.empty(); }

      private:
        /**
         * Convenience type for the buffer.  The general usage is primarily
         * FIFO-like, however truncation requires popping off both ends and word
         * wrapping requires iteration starting at both ends.  Functionally, a
         * std::list<> would work as well as a std::deque<>.  A std:vector<>
         * would work too, but would incur poor performance due to the nature of
         * how this will be used.
         */
        using fifo = std::deque<char_type>;

        fifo        _buf;           ///< Storage of the cell contents.
        std::size_t _width;         ///< Width of the column.
        std::size_t _written;       ///< Number of characters written for the current row.
        bool        _cell_start;    ///< Indicates if starting output to a cell.
        justify     _justify;       ///< Indicates left, right, or center justification.
        truncate    _truncate;      ///< Indicates truncation of contents.
        wrap        _wrap;          ///< Indicates character vs. word wrapping.
        pad_type    _pad;           ///< Holds the left and right padding strings.
        string_type _ellipsis;      ///< Holds the ellipsis string (used when truncating).

        /**
         * Outputs a line of spaces to fill the rest of the cell to the column width.
         *
         * @todo Fill character should be settable.
         *
         * @param sbuf  The streambuf to output spaces into.
         * @param c     The number of spaces to output.
         */
        static void draw_fill(rdbuf_type& sbuf, std::size_t c)
        {
            for (;c > 0; --c) { sbuf.sputc(' '); }
        }

        /**
         * Find the end of a line within the range of characters that fit within
         * a given width.  This counts multi-byte characters as one character.
         *
         * @todo Need to test on Window using MWVC and on Mac OSX to ensure EOL
         *       check works.
         *
         * @tparam Iterator     The iterator type
         *
         * @param begin         The starting iterator.
         * @param end           The ending iterator.
         * @param width         The maximum number of characters to include.
         *
         * @return              An iterator pointing to one past the last
         *                      character in the line.
         */
        template<typename Iterator>
        static Iterator find_line_end(Iterator begin, Iterator end, std::size_t width)
        {
            int cbr = 0;
            auto it = begin;
            while (width > 0 && it != end) {
                cbr += utf_char_score(*it);
                if (cbr == 0) {
                    --width;
                    if (*it == '\n') return it;
                }
                it = std::next(it);
            }
            return it;
        }

        /**
         * Find the position just past the last full word in the given range of
         * characters.  This uses std::isspace() to detect word boundaries.
         *
         * @tparam Iterator     The iterator type
         *
         * @param begin         The starting iterator.
         * @param end           The ending iterator.
         *
         * @return              An iterator pointing to one past the last
         *                      character of the last word in the line.  If a
         *                      whitespace character was not found then the end
         *                      iterator will be returned.
         */
        template<typename Iterator>
        static Iterator find_last_word(Iterator begin, Iterator end)
        {
            auto rbegin = std::make_reverse_iterator(end);
            auto rend = std::make_reverse_iterator(begin);
            auto rit =  std::find_if(rbegin, rend, [](char_type c) { return std::isspace(c); });
            if (rit == rend) return end;
            return std::next(rit).base();
        }

        /**
         * Find the end of a line within the range of characters that fit within
         * a given width.  This counts multi-byte characters as one character.
         *
         * @todo Need to test on Window using MWVC and on Mac OSX to ensure EOL
         *       check works.
         *
         * @tparam Iterator     The iterator type
         *
         * @param begin         The starting iterator.
         * @param end           The ending iterator.
         * @param width         The maximum number of characters to include.
         *
         * @return              An iterator pointing to one past the last
         *                      character in the line.
         */
        template<typename Iterator>
        Iterator find_output_end(Iterator begin, Iterator end, std::size_t width)
        {
            end = find_line_end(begin, end, width);
            if (_wrap == wrap::word) {
                if constexpr (std::is_same_v<Iterator, typename decltype(_buf)::iterator> ||
                              std::is_same_v<Iterator, typename decltype(_buf)::const_iterator>) {
                    if (end != _buf.end()) {
                        end = find_last_word(begin, std::next(end));
                        if (_written > 0 && (utflen(begin, end) > width)) { end = begin; }
                    }
                } else {
                    if (end != _buf.rend()) {
                        end = find_last_word(begin, std::next(end));
                        if (_written > 0 && (utflen(begin, end) > width)) { end = begin; }
                    }
                }
            }
            return end;
        }

        /**
         * Truncate the cell buffer contents to fit in a single line within the cell.
         *
         * @return  An iterator that is one past the last character to output
         *          regardless of whether the buffer is truncated or not.
         */
        auto truncate_buf()
        {
            if (_width == 0) {
                return std::find(_buf.cbegin(), _buf.cend(), '\n');

            } else if (_truncate == truncate::none) {
                return find_output_end(_buf.cbegin(), _buf.cend(), _width - _written);

            } else if (utflen(_buf.cbegin(), _buf.cend()) > _width) {
                auto width = _width - utflen(_ellipsis);
                if (_truncate == truncate::right) {
                    auto end = find_output_end(_buf.cbegin(), _buf.cend(), width);
                    while (_buf.cend() != end) { _buf.pop_back(); }
                    if (!empty()) _buf.insert(_buf.end(), _ellipsis.cbegin(), _ellipsis.cend());

                } else {
                    auto end = find_output_end(_buf.crbegin(), _buf.crend(), width);
                    while (_buf.crend() != end) { _buf.pop_front(); }
                    if (!empty()) _buf.insert(_buf.begin(), _ellipsis.cbegin(), _ellipsis.cend());
                }
            }
            return _buf.cend();
        }
#undef SLI
    };

    struct ASCII_DRAWING;
    struct BOX_DRAWING;

    /// The set of characters to use to draw lines around the table cells.
    struct style_info {
        /// The set of characters to draw a horizontal row.
        struct row {
            string_view_type _left;     ///< The character at the left most position in the row.
            string_view_type _center;   ///< The character where vertical lines meet horizontal lines.
            string_view_type _right;    ///< The character at the right most position in the row.
            string_view_type _line;     ///< The character for horizontal lines.
        };
        row _top;       ///< The characters for the top line of a table.
        row _middle;    ///< The characters for the line between rows of a table.
        row _bottom;    ///< The characters for the bottom line of a table.
        row _cell;      ///< The characters for vertical lines around columns in a table.
    };

    /// No line drawing around the table or between columns and rows.
    static constexpr style_info empty = {
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL, ASCII_DRAWING::NUL, ASCII_DRAWING::NUL },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL, ASCII_DRAWING::NUL, ASCII_DRAWING::NUL },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL, ASCII_DRAWING::NUL, ASCII_DRAWING::NUL },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL, ASCII_DRAWING::NUL, ASCII_DRAWING::NUL },
    };

    /// Use ASCII characters to draw boxes around the cells in the table.
    static constexpr style_info ascii = {
        { ASCII_DRAWING::PLUS, ASCII_DRAWING::PLUS, ASCII_DRAWING::PLUS, ASCII_DRAWING::DASH },
        { ASCII_DRAWING::PLUS, ASCII_DRAWING::PLUS, ASCII_DRAWING::PLUS, ASCII_DRAWING::DASH },
        { ASCII_DRAWING::PLUS, ASCII_DRAWING::PLUS, ASCII_DRAWING::PLUS, ASCII_DRAWING::DASH },
        { ASCII_DRAWING::PIPE, ASCII_DRAWING::PIPE, ASCII_DRAWING::PIPE, ASCII_DRAWING::NUL  }
    };

    /**
     * Use ASCII characters to format a table in markdown format.  (Set column
     * width to 0 for best rendering results.)
     */
    static constexpr style_info markdown = {
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL,  ASCII_DRAWING::NUL, ASCII_DRAWING::NUL  },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::PIPE, ASCII_DRAWING::NUL, ASCII_DRAWING::DASH },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL,  ASCII_DRAWING::NUL, ASCII_DRAWING::NUL  },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::PIPE, ASCII_DRAWING::NUL, ASCII_DRAWING::NUL  }
    };

    /// Use UTF box drawing characters to draw light lines around the cells in the table.
    static constexpr style_info box = {
        { BOX_DRAWING::LIGHT_DOWN_AND_RIGHT,     BOX_DRAWING::LIGHT_DOWN_AND_HORIZONTAL,     BOX_DRAWING::LIGHT_DOWN_AND_LEFT,     BOX_DRAWING::LIGHT_HORIZONTAL },
        { BOX_DRAWING::LIGHT_VERTICAL_AND_RIGHT, BOX_DRAWING::LIGHT_VERTICAL_AND_HORIZONTAL, BOX_DRAWING::LIGHT_VERTICAL_AND_LEFT, BOX_DRAWING::LIGHT_HORIZONTAL },
        { BOX_DRAWING::LIGHT_UP_AND_RIGHT,       BOX_DRAWING::LIGHT_UP_AND_HORIZONTAL,       BOX_DRAWING::LIGHT_UP_AND_LEFT,       BOX_DRAWING::LIGHT_HORIZONTAL },
        { BOX_DRAWING::LIGHT_VERTICAL,           BOX_DRAWING::LIGHT_VERTICAL,                BOX_DRAWING::LIGHT_VERTICAL,          ASCII_DRAWING::NUL                            }
    };

    /// Use UTF box drawing characters to draw heavy lines around the cells in the table.
    static constexpr style_info heavy_box = {
        { BOX_DRAWING::HEAVY_DOWN_AND_RIGHT,     BOX_DRAWING::HEAVY_DOWN_AND_HORIZONTAL,     BOX_DRAWING::HEAVY_DOWN_AND_LEFT,     BOX_DRAWING::HEAVY_HORIZONTAL },
        { BOX_DRAWING::HEAVY_VERTICAL_AND_RIGHT, BOX_DRAWING::HEAVY_VERTICAL_AND_HORIZONTAL, BOX_DRAWING::HEAVY_VERTICAL_AND_LEFT, BOX_DRAWING::HEAVY_HORIZONTAL },
        { BOX_DRAWING::HEAVY_UP_AND_RIGHT,       BOX_DRAWING::HEAVY_UP_AND_HORIZONTAL,       BOX_DRAWING::HEAVY_UP_AND_LEFT,       BOX_DRAWING::HEAVY_HORIZONTAL },
        { BOX_DRAWING::HEAVY_VERTICAL,           BOX_DRAWING::HEAVY_VERTICAL,                BOX_DRAWING::HEAVY_VERTICAL,          ASCII_DRAWING::NUL                            }
    };

    /// Use UTF box drawing characters to draw double lines around the cells in the table.
    static constexpr style_info double_box = {
        { BOX_DRAWING::DOUBLE_DOWN_AND_RIGHT,     BOX_DRAWING::DOUBLE_DOWN_AND_HORIZONTAL,     BOX_DRAWING::DOUBLE_DOWN_AND_LEFT,     BOX_DRAWING::DOUBLE_HORIZONTAL },
        { BOX_DRAWING::DOUBLE_VERTICAL_AND_RIGHT, BOX_DRAWING::DOUBLE_VERTICAL_AND_HORIZONTAL, BOX_DRAWING::DOUBLE_VERTICAL_AND_LEFT, BOX_DRAWING::DOUBLE_HORIZONTAL },
        { BOX_DRAWING::DOUBLE_UP_AND_RIGHT,       BOX_DRAWING::DOUBLE_UP_AND_HORIZONTAL,       BOX_DRAWING::DOUBLE_UP_AND_LEFT,       BOX_DRAWING::DOUBLE_HORIZONTAL },
        { BOX_DRAWING::DOUBLE_VERTICAL,           BOX_DRAWING::DOUBLE_VERTICAL,                BOX_DRAWING::DOUBLE_VERTICAL,          ASCII_DRAWING::NUL                            }
    };

    /**
     * Use UTF box drawing characters to draw light lines with rounded corners
     * around the cells in the table.
     */
    static constexpr style_info rounded_box = {
        { BOX_DRAWING::LIGHT_ARC_DOWN_AND_RIGHT, BOX_DRAWING::LIGHT_DOWN_AND_HORIZONTAL,     BOX_DRAWING::LIGHT_ARC_DOWN_AND_LEFT, BOX_DRAWING::LIGHT_HORIZONTAL },
        { BOX_DRAWING::LIGHT_VERTICAL_AND_RIGHT, BOX_DRAWING::LIGHT_VERTICAL_AND_HORIZONTAL, BOX_DRAWING::LIGHT_VERTICAL_AND_LEFT, BOX_DRAWING::LIGHT_HORIZONTAL },
        { BOX_DRAWING::LIGHT_ARC_UP_AND_RIGHT,   BOX_DRAWING::LIGHT_UP_AND_HORIZONTAL,       BOX_DRAWING::LIGHT_ARC_UP_AND_LEFT,   BOX_DRAWING::LIGHT_HORIZONTAL },
        { BOX_DRAWING::LIGHT_VERTICAL,           BOX_DRAWING::LIGHT_VERTICAL,                BOX_DRAWING::LIGHT_VERTICAL,          ASCII_DRAWING::NUL                            }
    };

    /**
     * Use ASCII characters to draw boxes around the cells in the table
     * excluding the outer edges.
     */
    static constexpr style_info borderless_ascii = {
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL,  ASCII_DRAWING::NUL, ASCII_DRAWING::NUL  },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::PLUS, ASCII_DRAWING::NUL, ASCII_DRAWING::DASH },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL,  ASCII_DRAWING::NUL, ASCII_DRAWING::NUL  },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::PIPE, ASCII_DRAWING::NUL, ASCII_DRAWING::NUL  }
    };

    /**
     * Use UTF box drawing characters to draw light lines around the cells in
     * the table excluding the outer edges.
     */
    static constexpr style_info borderless_box = {
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL,                         ASCII_DRAWING::NUL, ASCII_DRAWING::NUL            },
        { ASCII_DRAWING::NUL, BOX_DRAWING::LIGHT_VERTICAL_AND_HORIZONTAL, ASCII_DRAWING::NUL, BOX_DRAWING::LIGHT_HORIZONTAL },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL,                         ASCII_DRAWING::NUL, ASCII_DRAWING::NUL            },
        { ASCII_DRAWING::NUL, BOX_DRAWING::LIGHT_VERTICAL,                ASCII_DRAWING::NUL, ASCII_DRAWING::NUL            }
    };

    /**
     * Use UTF box drawing characters to draw double lines around the cells in
     * the table excluding the outer edges.
     */
    static constexpr style_info borderless_double_box = {
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL,                          ASCII_DRAWING::NUL, ASCII_DRAWING::NUL             },
        { ASCII_DRAWING::NUL, BOX_DRAWING::DOUBLE_VERTICAL_AND_HORIZONTAL, ASCII_DRAWING::NUL, BOX_DRAWING::DOUBLE_HORIZONTAL },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL,                          ASCII_DRAWING::NUL, ASCII_DRAWING::NUL             },
        { ASCII_DRAWING::NUL, BOX_DRAWING::DOUBLE_VERTICAL,                ASCII_DRAWING::NUL, ASCII_DRAWING::NUL             }
    };

    /**
     * Use UTF box drawing characters to draw heavy lines around the cells in
     * the table excluding the outer edges.
     */
    static constexpr style_info borderless_heavy_box = {
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL,                         ASCII_DRAWING::NUL, ASCII_DRAWING::NUL            },
        { ASCII_DRAWING::NUL, BOX_DRAWING::HEAVY_VERTICAL_AND_HORIZONTAL, ASCII_DRAWING::NUL, BOX_DRAWING::HEAVY_HORIZONTAL },
        { ASCII_DRAWING::NUL, ASCII_DRAWING::NUL,                         ASCII_DRAWING::NUL, ASCII_DRAWING::NUL            },
        { ASCII_DRAWING::NUL, BOX_DRAWING::HEAVY_VERTICAL,                ASCII_DRAWING::NUL, ASCII_DRAWING::NUL            }
    };


    /**
     * Constructor.
     *
     * This installs itself as the streambuf for the specified output stream. It
     * keeps track of the original streambuf for flushing the formatted output.
     * It uses the RAII idiom and restores the original streambuf when the
     * current instance is destroyed.
     *
     * @param os        The output stream to affect.
     * @param cells     R-value reference to a list of cell_info configurations.
     */
    basic_tabulator(ostream_type& os, cells_type&& cells):
        _os(os),
        _cells(std::move(cells)),
        _column(0),
        _sync_column(0),
        _line_start(true),
        _real_sb(os.rdbuf()),
        _style(ascii)
    {
        _os.rdbuf(this);
    }

    /**
     * Constructor.
     *
     * This installs itself as the streambuf for the specified output stream. It
     * keeps track of the original streambuf for flushing the formatted output.
     * It uses the RAII idiom and restores the original streambuf when the
     * current instance is destroyed.
     *
     * @param os        The output stream to affect.
     * @param cells     L-value reference to a list of cell_info configurations.
     */
    basic_tabulator(ostream_type& os, cells_type const& cells):
        _os(os),
        _cells(cells),
        _column(0),
        _sync_column(0),
        _real_sb(os.rdbuf()),
        _style(ascii)
    {
        _os.rdbuf(this);
    }

    /**
     * Destructor.
     *
     * This restores the original streambuf to the output stream.
     */
    virtual ~basic_tabulator() { _os.rdbuf(_real_sb); }

    /**
     * Get the current column number (0 is the first column).
     *
     * @return      The current column number.
     */
    auto get_current_column() const noexcept { return _column; }

    /**
     * Get the current cell.  This is useful for chaining several calls to
     * change aspects about the current cell column.
     *
     * @return      A reference to the current cell.
     */
    auto& get_current_cell() noexcept { return _cells[_column]; }

    /**
     * Set the box drawing style for the table.  This form is useful for
     * chaining other set_*() functions.
     *
     * @param s     The style to set.
     *
     * @return      Reference to `*this` tabulator.
     */
    auto& set_style(style_info const& s) noexcept { _style = s; return *this; }

    /**
     * Set current cell column width.  This form is useful for chaining other
     * set_*() functions.
     *
     * @param w     The cell column width to set.
     *
     * @return      Reference to `*this` tabulator.
     */
    auto& set_width(std::size_t w)  noexcept { get_current_cell().set_width(w); return *this; }

    /**
     * Set current cell column justification.  This form is useful for chaining
     * other set_*() functions.
     *
     * @param j     The cell column justification to set.
     *
     * @return      Reference to `*this` tabulator.
     */
    auto& set_justify(justify j) noexcept { get_current_cell().set_justify(j); return *this; }

    /**
     * Set current cell column truncation.  This form is useful for chaining
     * other set_*() functions.
     *
     * @param t     The cell column truncation to set.
     *
     * @return      Reference to `*this` tabulator.
     */
    auto& set_truncate(truncate t) noexcept { get_current_cell().set_truncate(t); return *this; }

    /**
     * Set current cell column wrapping.  This form is useful for chaining other
     * set_*() functions.
     *
     * @param w     The cell column wrapping to set.
     *
     * @return      Reference to `*this` tabulator.
     */
    auto& set_wrap(wrap w) noexcept { get_current_cell().set_wrap(w); return *this; }

    /**
     * Set current cell column padding.  This form is useful for chaining other
     * set_*() functions.
     *
     * @param p     A tuple with the left and right cell column padding to set.
     *
     * @return      Reference to `*this` tabulator.
     */
    auto& set_pad(std::tuple<string_type, string_type>&& p) noexcept {
        get_current_cell().set_pad(std::move(p));
        return *this;
    }

    /**
     * Set current cell column padding.  This form is useful for chaining other
     * set_*() functions.
     *
     * @param p     A tuple with the left and right cell column padding to set.
     *
     * @return      Reference to `*this` tabulator.
     */
    auto& set_pad(std::tuple<string_type, string_type> const& p) noexcept {
        get_current_cell().set_pad(p);
        return *this;
    }

    /**
     * Set current cell column ellipsis.  This form is useful for chaining other
     * set_*() functions.
     *
     * @param e     The cell column ellipsis to set.
     *
     * @return      Reference to `*this` tabulator.
     */
    auto& set_ellipsis(string_type&& e) noexcept {
        get_current_cell().set_ellipsis(std::move(e));
        return *this;
    }

    /**
     * Set current cell column ellipsis.  This form is useful for chaining other
     * set_*() functions.
     *
     * @param e     The cell column ellipsis to set.
     *
     * @return      Reference to `*this` tabulator.
     */
    auto& set_ellipsis(string_view_type e) noexcept {
        get_current_cell().set_ellipsis(e);
        return *this;
    }

    /// Draw the top line of the table.
    void top_line() { draw_line(_style._top); }

    /// Draw a horizontal line between rows of the table.
    void horiz_line() { draw_line(_style._middle); }

    /// Draw the bottom line of the table.
    void bottom_line() { draw_line(_style._bottom); }

    /**
     * Select the next column for buffering output characters.  If the current
     * column is the last column, flush the current row to the wrapped streambuf
     * and advance back to the first column.
     */
    void next_column()
    {
        assert(_sync_column < _cells.size());
        assert(_column < _cells.size());
        ++_column;
        if (_column == _cells.size()) {
            _column = 0;
            flush(true);
        }
        assert(_sync_column < _cells.size());
        assert(_column < _cells.size());
    }

  protected:
    /**
     * Override streambuf base class's implementation of overflow.
     *
     * @param ch    An integer containing the character or EOF.
     *
     * @return      The character or EOF.
     */
    int_type overflow(int_type ch) override
    {
        if (Traits::not_eof(ch)) { _cells[_column].putc(Traits::to_char_type(ch)); }
        return ch;
    }

    /**
     * Override streambuf base class's implementation of sync.
     *
     * @return      Indication of success or failure:
     *              -  0: success
     *              - -1: failure
     */
    int sync() override
    {
        flush(false);
        return _real_sb->pubsync();
    }

  protected:
    ostream_type& _os;          ///< The output stream whose streambuf is being wrapped.

  private:
    cells_type    _cells;       ///< Information about all the columns.
    std::size_t   _column;      ///< The current column being filled.
    std::size_t   _sync_column; ///< The last synced column.
    bool          _line_start;
    base_type*    _real_sb;     ///< The original wrapped streambuf.
    style_info    _style;       ///< The current table line drawing style information.


    void draw_column_sep(string_view_type vline)
    {
        if (!vline.empty()) { _real_sb->sputn(vline.data(), vline.size()); }
    }

    /**
     * Send all the output collected in each of the cells to the underlying streambuf.
     */
    void flush(bool all_cells)
    {
        assert(_sync_column < _cells.size());
        assert(_column < _cells.size());
        auto any = [] (auto&& begin, auto&& end) {
                       return std::any_of(begin, end, [] (cell_info const& c) {
                                                          return !c.empty();
                                                      });
                   };
        auto more = [this, all_cells, &any]() {
                        if ((!all_cells && _sync_column < _column) ||
                            (all_cells && _sync_column > 0)) {
                            return true;
                        }

                        auto begin = _cells.begin();
                        if (!all_cells) {
                            std::advance(begin, _sync_column);
                        }
                        return any(begin, _cells.end());
                    };

        assert(_sync_column < _cells.size());
        assert(_column < _cells.size());
        while (more()) {
            if (_line_start) {
                draw_column_sep(_style._cell._left);
                _line_start = false;
            }

            auto next = std::next(_cells.begin(), _sync_column + 1);
            auto full_cell = all_cells || ((_sync_column != _column) || any(next, _cells.end()));

            assert(_sync_column < _cells.size());
            assert(_column < _cells.size());
            full_cell = _cells[_sync_column].write_line(*_real_sb, full_cell);

            if (full_cell) {
                assert(_sync_column < _cells.size());
                assert(_column < _cells.size());
                if ((_sync_column + 1) == _cells.size()) {
                    draw_column_sep(_style._cell._right);
                    _real_sb->sputc('\n');
                    _line_start = true;
                } else {
                    draw_column_sep(_style._cell._center);
                }

                ++_sync_column;
                if (_sync_column == _cells.size()) { _sync_column = 0; }
                assert(_sync_column < _cells.size());
                assert(_column < _cells.size());
            }
        }
    }

    /**
     * Outputs a part of a horizontal line for a table to the wrapped streambuf.
     * It outputs the left edge and the line characters to cover the current
     * column's width.
     *
     * @param cell  Reference to the relevant column information.
     * @param left  The left edge of the segment to output.
     * @param line  The line characters to output.
     */
    void draw_segment(cell_info const& cell, string_view_type left, string_view_type line)
    {
        _real_sb->sputn(left.data(), left.size());
        if (!line.empty())
        {
            auto const& [lpad, rpad] = cell.get_pad();
            auto width = (cell.get_width() +
                          utflen(string_view_type(lpad)) +
                          utflen(string_view_type(rpad)));

            std::size_t i = 0;
            int score = 0;
            while (width > 0) {
                score += utf_char_score(line[i]);
                _real_sb->sputc(line[i]);
                ++i;
                if (i == line.size()) { i = 0; }
                if (score == 0) { --width; }
            }
        }
    }

    using row_type = typename style_info::row;  /// < Convenience type.

    /**
     * Draws a horizontal line for a table based on the characters for the
     * specified row.
     *
     * @param row   The set of characters used for drawing a horizontal line.
     */
    void draw_line(row_type const& row)
    {
        assert(_sync_column < _cells.size());
        assert(_column < _cells.size());
        if (_column != 0) {
            _column = 0;
            flush(true);
        }
        assert(_sync_column < _cells.size());
        assert(_column < _cells.size());

        draw_segment(_cells.front(), row._left, row._line);

        auto const& line = row._line;
        auto const& tee = row._center;
        ::std::for_each(::std::next(_cells.begin()), _cells.end(),
                        [this, line, tee](cell_info const& c)
                        {
                            draw_segment(c, tee, line);
                        });
        _real_sb->sputn(row._right.data(), row._right.size());
        _real_sb->sputc('\n');
    }


    static constexpr string_view_type string_literal_init(std::string_view s [[maybe_unused]],
                                                          std::wstring_view ws [[maybe_unused]],
                                                          std::u16string_view u16s [[maybe_unused]],
                                                          std::u32string_view u32s [[maybe_unused]]) {
        if constexpr (std::is_same_v<char_type, char>) { return s; }
        else if constexpr (std::is_same_v<char_type, wchar_t>) { return ws; }
        else if constexpr (std::is_same_v<char_type, char16_t>) { return u16s; }
        else if constexpr (std::is_same_v<char_type, char32_t>) { return u32s; }
    }

};

extern template class basic_tabulator<char>;
extern template class basic_tabulator<wchar_t>;
extern template class basic_tabulator<char16_t>;
extern template class basic_tabulator<char32_t>;

/// Type alias for a tabulator filter using ordinary chars.
using tabulator    = basic_tabulator<char>;

/// Type alias for a tabulator filter using wchar_t.
using wtabulator   = basic_tabulator<wchar_t>;

/// Type alias for a tabulator filter using char16_t.
using u16tabulator = basic_tabulator<char16_t>;

/// Type alias for a tabulator filter using char32_t.
using u32tabulator = basic_tabulator<char32_t>;

/**
 * Determine if the passed in streambuf pointer points to a tabulator streambuf
 * or not.
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 *
 * @param buf       Pointer to the streambuf to test.
 *
 * @return          Whether the streambuf pointer points to a tabulator streambuf:
 *                  - 'true': *buf refers to a tabulator streambuf.
 *                  - 'false': *buf refers to some other kind of streambuf.
 */
template <class CharT, class Traits>
bool is_tabulator_streambuf(std::basic_streambuf<CharT, Traits>* buf)
{
    return typeid(*buf) == typeid(basic_tabulator<CharT, Traits>);
}

namespace _details {
/// @cond INTERNAL
/**
 * Template class that enables passing IO manipulator function
 * settings to the underlying streambuf.
 *
 * @tparam T        The type of the datat to be passed to the underlying streambuf.
 */
template <typename T>
struct cell_param {
    T v;        ///< The value to be passed to the underlying streambuf.
};

/**
 * Convenience function for creating cell_param of the right type objects with
 * the desired value.
 *
 * @tparam T        The type of the datat to be passed to the underlying streambuf.
 *
 * @param v         The value to be passed to the underlying streambuf.
 *
 * @return          A cell_param<T> instance containing the value from v.
 */
template <typename T>
cell_param<std::remove_reference_t<T>> set_cell_param(T&& v)
{
    return { std::forward<T>(v) };
}
/// @endcond
}

/**
 * Set current cell column justification.
 *
 * @param j     The cell column justification to set.
 *
 * @return      A cell_param<justify> value for passing the output stream
 *              operator<<() overload.
 */
inline auto set_justify(justify j) { return _details::set_cell_param(j); }

/**
 * Set current cell column truncation.
 *
 * @param t     The cell column truncation to set.
 *
 * @return      A cell_param<truncate> value for passing the output stream
 *              operator<<() overload.
 */
inline auto set_truncate(truncate t) { return _details::set_cell_param(t); }

/**
 * Set current cell column wrapping.
 *
 * @param w     The cell column wrapping to set.
 *
 * @return      A cell_param<wrap> value for passing the output stream
 *              operator<<() overload.
 */
inline auto set_wrap(wrap w) { return _details::set_cell_param(w); }

/**
 * Set current cell column width.
 *
 * @param w     The cell column width to set.
 *
 * @return      A cell_param<std::size_t> value for passing the output stream
 *              operator<<() overload.
 */
inline auto set_width(std::size_t w) { return _details::set_cell_param(w); }

/**
 * Set table style using ordinary chars.
 *
 * @param s     The table style to set.
 *
 * @return      A cell_param<tabulator::style_info> value for passing the
 *              output stream operator<<() overload.
 */
inline auto set_style(typename tabulator::style_info const& s) {
    return _details::set_cell_param(s);
}

/**
 * Set table style using wchar_t.
 *
 * @param s     The table style to set.
 *
 * @return      A cell_param<wtabulator::style_info> value for passing the
 *              output stream operator<<() overload.
 */
inline auto set_style(typename wtabulator::style_info const& s) {
    return _details::set_cell_param(s);
}

/**
 * Set table style using char16_t.
 *
 * @param s     The table style to set.
 *
 * @return      A cell_param<u16tabulator::style_info> value for passing the
 *              output stream operator<<() overload.
 */
inline auto set_style(typename u16tabulator::style_info const& s) {
    return _details::set_cell_param(s);
}

/**
 * Set table style using char32_t.
 *
 * @param s     The table style to set.
 *
 * @return      A cell_param<u32tabulator::style_info> value for passing the
 *              output stream operator<<() overload.
 */
inline auto set_style(typename u32tabulator::style_info const& s) {
    return _details::set_cell_param(s);
}

/**
 * Set the current cell padding from forwarding references to std::basic_string<>.
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 *
 * @param lpad      Characters for padding on the left.
 * @param rpad      Characters for padding on the left.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
template <class CharT, class Traits>
inline auto set_pad(std::basic_string<CharT, Traits>&& lpad,
                    std::basic_string<CharT, Traits>&& rpad)
{
    return _details::set_cell_param(std::make_tuple(std::forward(lpad), std::forward(rpad)));
}

/**
 * Set the current cell padding using template specified character type.
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 *
 * @param lpad      Characters for padding on the left.
 * @param rpad      Characters for padding on the left.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
template <class CharT, class Traits = std::char_traits<CharT>>
inline auto basic_set_pad(std::basic_string_view<CharT, Traits> lpad,
                          std::basic_string_view<CharT, Traits> rpad)
{
    using st = std::basic_string<CharT, Traits>;
    return _details::set_cell_param(std::make_tuple(st(lpad), st(rpad)));
}

/**
 * Set the current cell padding using ordinary chars.
 *
 * @param lpad      Characters for padding on the left.
 * @param rpad      Characters for padding on the left.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
inline auto set_pad(std::string_view lpad, std::string_view rpad) {
    return basic_set_pad(lpad, rpad);
}

/**
 * Set the current cell padding using wchar_t.
 *
 * @param lpad      Characters for padding on the left.
 * @param rpad      Characters for padding on the left.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
inline auto set_pad(std::wstring_view lpad, std::wstring_view rpad) {
    return basic_set_pad(lpad, rpad);
}

/**
 * Set the current cell padding using char16_t.
 *
 * @param lpad      Characters for padding on the left.
 * @param rpad      Characters for padding on the left.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
inline auto set_pad(std::u16string_view lpad, std::u16string_view rpad) {
    return basic_set_pad(lpad, rpad);
}

/**
 * Set the current cell padding using char32_t.
 *
 * @param lpad      Characters for padding on the left.
 * @param rpad      Characters for padding on the left.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
inline auto set_pad(std::u32string_view lpad, std::u32string_view rpad) {
    return basic_set_pad(lpad, rpad);
}

/**
 * Set the ellipsis for the current cell from forwarding references to std::basic_string<>.
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 *
 * @param ellipsis  Characters for the ellipsis.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
template <class CharT, class Traits>
inline auto set_ellipsis(std::basic_string<CharT, Traits>&& ellipsis)
{
    return _details::set_cell_param(std::move(ellipsis));
}

/**
 * Set the ellipsis for the current cell using template specified character type.
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 *
 * @param ellipsis  Characters for the ellipsis.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
template <class CharT, class Traits>
inline auto basic_set_ellipsis(std::basic_string_view<CharT, Traits> ellipsis)
{
    using st = std::basic_string<CharT, Traits>;
    return _details::set_cell_param(st(ellipsis));
}

/**
 * Set the ellipsis for the current cell using ordinary chars.
 *
 * @param ellipsis  Characters for the ellipsis.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
inline auto set_ellipsis(std::string_view ellipsis) { return basic_set_ellipsis(ellipsis); }

/**
 * Set the ellipsis for the current cell using wchar_t.
 *
 * @param ellipsis  Characters for the ellipsis.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
inline auto set_ellipsis(std::wstring_view ellipsis) { return basic_set_ellipsis(ellipsis); }

/**
 * Set the ellipsis for the current cell using char16_t.
 *
 * @param ellipsis  Characters for the ellipsis.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
inline auto set_ellipsis(std::u16string_view ellipsis) { return basic_set_ellipsis(ellipsis); }

/**
 * Set the ellipsis for the current cell using char32_t.
 *
 * @param ellipsis  Characters for the ellipsis.
 *
 * @return          A cell_param<std::tuple<std::basic_string<>, std::basic_string<>>>
 *                  value for passing the output stream operator<<() overload.
 */
inline auto set_ellipsis(std::u32string_view ellipsis) { return basic_set_ellipsis(ellipsis); }

/**
 * Left shift operator overload for passing the various settings to a tabulator
 * streambuf instance.  If the output stream uses a streambuf other than a
 * tabulator streambuf, then the operation will be a NOP.
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 *
 * @param os        Reference to the output stream to send the settings to.
 * @param p         The setting parameter to pass to the underlying streambuf.
 *
 * @return          The reference to the output stream.
 */
template <class CharT, class Traits, typename T>
std::basic_ostream<CharT, Traits>& operator<< (std::basic_ostream<CharT, Traits>& os,
                                               _details::cell_param<T>&& p)
{
    using tab = basic_tabulator<CharT, Traits>;
    using st = std::basic_string<CharT, Traits>;
    using style_info = typename basic_tabulator<CharT, Traits>::style_info;
    using pad_t = std::tuple<st, st>;
    using pt = std::remove_reference_t<std::remove_cv_t<T>>;

    if (is_tabulator_streambuf(os.rdbuf())) {
        auto buf [[maybe_unused]] = reinterpret_cast<tab*>(os.rdbuf());
        if      constexpr (std::is_same_v<pt, justify>)     { buf->set_justify (std::move(p.v)); }
        else if constexpr (std::is_same_v<pt, truncate>)    { buf->set_truncate(std::move(p.v)); }
        else if constexpr (std::is_same_v<pt, wrap>)        { buf->set_wrap    (std::move(p.v)); }
        else if constexpr (std::is_same_v<pt, std::size_t>) { buf->set_width   (std::move(p.v)); }
        else if constexpr (std::is_same_v<pt, pad_t>)       { buf->set_pad     (std::move(p.v)); }
        else if constexpr (std::is_same_v<pt, style_info>)  { buf->set_style   (std::move(p.v)); }
    }
    return os;
}

/**
 * IO manipulator to indicate the end of a cell/column.  This is similar to std::endl.
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 *
 * @param os        Reference to the output stream to send the settings to.
 *
 * @return          The reference to the output stream.
 */
template <class CharT, class Traits = std::char_traits<CharT>>
std::basic_ostream<CharT, Traits>& endc(std::basic_ostream<CharT, Traits>& os)
{
    if (is_tabulator_streambuf(os.rdbuf())) {
        auto buf = reinterpret_cast<basic_tabulator<CharT, Traits>*>(os.rdbuf());
        buf->next_column();
    }
    return os;
}

/**
 * IO manipulator to draw the top line of a table
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 *
 * @param os        Reference to the output stream to send the settings to.
 *
 * @return          The reference to the output stream.
 */
template <class CharT, class Traits = std::char_traits<CharT>>
std::basic_ostream<CharT, Traits>& top_line(std::basic_ostream<CharT, Traits>& os)
{
    if (is_tabulator_streambuf(os.rdbuf())) {
        auto buf = reinterpret_cast<basic_tabulator<CharT, Traits>*>(os.rdbuf());
        buf->top_line();
    }
    return os;
}

/**
 * IO manipulator to draw a horizontal line between rows of a table
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 *
 * @param os        Reference to the output stream to send the settings to.
 *
 * @return          The reference to the output stream.
 */
template <class CharT, class Traits = std::char_traits<CharT>>
std::basic_ostream<CharT, Traits>& horiz_line(std::basic_ostream<CharT, Traits>& os)
{
    if (is_tabulator_streambuf(os.rdbuf())) {
        auto buf = reinterpret_cast<basic_tabulator<CharT, Traits>*>(os.rdbuf());
        buf->horiz_line();
    }
    return os;
}

/**
 * IO manipulator to draw the bottom line of a table
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 *
 * @param os        Reference to the output stream to send the settings to.
 *
 * @return          The reference to the output stream.
 */
template <class CharT, class Traits = std::char_traits<CharT>>
std::basic_ostream<CharT, Traits>& bottom_line(std::basic_ostream<CharT, Traits>& os)
{
    if (is_tabulator_streambuf(os.rdbuf())) {
        auto buf = reinterpret_cast<basic_tabulator<CharT, Traits>*>(os.rdbuf());
        buf->bottom_line();
    }
    return os;
}

#define SLI(x) string_literal_init(x, L##x, u##x, U##x)

/**
 * ASCII box drawing characters
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 */
template <typename CharT, typename Traits>
struct basic_tabulator<CharT, Traits>::ASCII_DRAWING {
    static constexpr auto NUL   = SLI("");      ///< No symbol.
    static constexpr auto SPACE = SLI(" ");     ///< Space ASCII symbol.
    static constexpr auto PLUS  = SLI("+");     ///< Plus ASCII symbol.
    static constexpr auto PIPE  = SLI("|");     ///< Pipe of vertical bar ASCII symbol.
    static constexpr auto DASH  = SLI("-");     ///< Dash or minus ASCII symbol.
};

/**
 * UTF box drawing characters
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 */
template <typename CharT, typename Traits>
struct basic_tabulator<CharT, Traits>::BOX_DRAWING {
    static constexpr auto LIGHT_HORIZONTAL                         = SLI("\u2500"); ///< "─" U+2500
    static constexpr auto HEAVY_HORIZONTAL                         = SLI("\u2501"); ///< "━" U+2501
    static constexpr auto LIGHT_VERTICAL                           = SLI("\u2502"); ///< "│" U+2502
    static constexpr auto HEAVY_VERTICAL                           = SLI("\u2503"); ///< "┃" U+2503
    static constexpr auto LIGHT_TRIPLE_DASH_HORIZONTAL             = SLI("\u2504"); ///< "┄" U+2504
    static constexpr auto HEAVY_TRIPLE_DASH_HORIZONTAL             = SLI("\u2505"); ///< "┅" U+2505
    static constexpr auto LIGHT_TRIPLE_DASH_VERTICAL               = SLI("\u2506"); ///< "┆" U+2506
    static constexpr auto HEAVY_TRIPLE_DASH_VERTICAL               = SLI("\u2507"); ///< "┇" U+2507
    static constexpr auto LIGHT_QUADRUPLE_DASH_HORIZONTAL          = SLI("\u2508"); ///< "┈" U+2508
    static constexpr auto HEAVY_QUADRUPLE_DASH_HORIZONTAL          = SLI("\u2509"); ///< "┉" U+2509
    static constexpr auto LIGHT_QUADRUPLE_DASH_VERTICAL            = SLI("\u250A"); ///< "┊" U+250A
    static constexpr auto HEAVY_QUADRUPLE_DASH_VERTICAL            = SLI("\u250B"); ///< "┋" U+250B
    static constexpr auto LIGHT_DOWN_AND_RIGHT                     = SLI("\u250C"); ///< "┌" U+250C
    static constexpr auto DOWN_LIGHT_AND_RIGHT_HEAVY               = SLI("\u250D"); ///< "┍" U+250D
    static constexpr auto DOWN_HEAVY_AND_RIGHT_LIGHT               = SLI("\u250E"); ///< "┎" U+250E
    static constexpr auto HEAVY_DOWN_AND_RIGHT                     = SLI("\u250F"); ///< "┏" U+250F
    static constexpr auto LIGHT_DOWN_AND_LEFT                      = SLI("\u2510"); ///< "┐" U+2510
    static constexpr auto DOWN_LIGHT_AND_LEFT_HEAVY                = SLI("\u2511"); ///< "┑" U+2511
    static constexpr auto DOWN_HEAVY_AND_LEFT_LIGHT                = SLI("\u2512"); ///< "┒" U+2512
    static constexpr auto HEAVY_DOWN_AND_LEFT                      = SLI("\u2513"); ///< "┓" U+2513
    static constexpr auto LIGHT_UP_AND_RIGHT                       = SLI("\u2514"); ///< "└" U+2514
    static constexpr auto UP_LIGHT_AND_RIGHT_HEAVY                 = SLI("\u2515"); ///< "┕" U+2515
    static constexpr auto UP_HEAVY_AND_RIGHT_LIGHT                 = SLI("\u2516"); ///< "┖" U+2516
    static constexpr auto HEAVY_UP_AND_RIGHT                       = SLI("\u2517"); ///< "┗" U+2517
    static constexpr auto LIGHT_UP_AND_LEFT                        = SLI("\u2518"); ///< "┘" U+2518
    static constexpr auto UP_LIGHT_AND_LEFT_HEAVY                  = SLI("\u2519"); ///< "┙" U+2519
    static constexpr auto UP_HEAVY_AND_LEFT_LIGHT                  = SLI("\u251A"); ///< "┚" U+251A
    static constexpr auto HEAVY_UP_AND_LEFT                        = SLI("\u251B"); ///< "┛" U+251B
    static constexpr auto LIGHT_VERTICAL_AND_RIGHT                 = SLI("\u251C"); ///< "├" U+251C
    static constexpr auto VERTICAL_LIGHT_AND_RIGHT_HEAVY           = SLI("\u251D"); ///< "┝" U+251D
    static constexpr auto UP_HEAVY_AND_RIGHT_DOWN_LIGHT            = SLI("\u251E"); ///< "┞" U+251E
    static constexpr auto DOWN_HEAVY_AND_RIGHT_UP_LIGHT            = SLI("\u251F"); ///< "┟" U+251F
    static constexpr auto VERTICAL_HEAVY_AND_RIGHT_LIGHT           = SLI("\u2520"); ///< "┠" U+2520
    static constexpr auto DOWN_LIGHT_AND_RIGHT_UP_HEAVY            = SLI("\u2521"); ///< "┡" U+2521
    static constexpr auto UP_LIGHT_AND_RIGHT_DOWN_HEAVY            = SLI("\u2522"); ///< "┢" U+2522
    static constexpr auto HEAVY_VERTICAL_AND_RIGHT                 = SLI("\u2523"); ///< "┣" U+2523
    static constexpr auto LIGHT_VERTICAL_AND_LEFT                  = SLI("\u2524"); ///< "┤" U+2524
    static constexpr auto VERTICAL_LIGHT_AND_LEFT_HEAVY            = SLI("\u2525"); ///< "┥" U+2525
    static constexpr auto UP_HEAVY_AND_LEFT_DOWN_LIGHT             = SLI("\u2526"); ///< "┦" U+2526
    static constexpr auto DOWN_HEAVY_AND_LEFT_UP_LIGHT             = SLI("\u2527"); ///< "┧" U+2527
    static constexpr auto VERTICAL_HEAVY_AND_LEFT_LIGHT            = SLI("\u2528"); ///< "┨" U+2528
    static constexpr auto DOWN_LIGHT_AND_LEFT_UP_HEAVY             = SLI("\u2529"); ///< "┩" U+2529
    static constexpr auto UP_LIGHT_AND_LEFT_DOWN_HEAVY             = SLI("\u252A"); ///< "┪" U+252A
    static constexpr auto HEAVY_VERTICAL_AND_LEFT                  = SLI("\u252B"); ///< "┫" U+252B
    static constexpr auto LIGHT_DOWN_AND_HORIZONTAL                = SLI("\u252C"); ///< "┬" U+252C
    static constexpr auto LEFT_HEAVY_AND_RIGHT_DOWN_LIGHT          = SLI("\u252D"); ///< "┭" U+252D
    static constexpr auto RIGHT_HEAVY_AND_LEFT_DOWN_LIGHT          = SLI("\u252E"); ///< "┮" U+252E
    static constexpr auto DOWN_LIGHT_AND_HORIZONTAL_HEAVY          = SLI("\u252F"); ///< "┯" U+252F
    static constexpr auto DOWN_HEAVY_AND_HORIZONTAL_LIGHT          = SLI("\u2530"); ///< "┰" U+2530
    static constexpr auto RIGHT_LIGHT_AND_LEFT_DOWN_HEAVY          = SLI("\u2531"); ///< "┱" U+2531
    static constexpr auto LEFT_LIGHT_AND_RIGHT_DOWN_HEAVY          = SLI("\u2532"); ///< "┲" U+2532
    static constexpr auto HEAVY_DOWN_AND_HORIZONTAL                = SLI("\u2533"); ///< "┳" U+2533
    static constexpr auto LIGHT_UP_AND_HORIZONTAL                  = SLI("\u2534"); ///< "┴" U+2534
    static constexpr auto LEFT_HEAVY_AND_RIGHT_UP_LIGHT            = SLI("\u2535"); ///< "┵" U+2535
    static constexpr auto RIGHT_HEAVY_AND_LEFT_UP_LIGHT            = SLI("\u2536"); ///< "┶" U+2536
    static constexpr auto UP_LIGHT_AND_HORIZONTAL_HEAVY            = SLI("\u2537"); ///< "┷" U+2537
    static constexpr auto UP_HEAVY_AND_HORIZONTAL_LIGHT            = SLI("\u2538"); ///< "┸" U+2538
    static constexpr auto RIGHT_LIGHT_AND_LEFT_UP_HEAVY            = SLI("\u2539"); ///< "┹" U+2539
    static constexpr auto LEFT_LIGHT_AND_RIGHT_UP_HEAVY            = SLI("\u253A"); ///< "┺" U+253A
    static constexpr auto HEAVY_UP_AND_HORIZONTAL                  = SLI("\u253B"); ///< "┻" U+253B
    static constexpr auto LIGHT_VERTICAL_AND_HORIZONTAL            = SLI("\u253C"); ///< "┼" U+253C
    static constexpr auto LEFT_HEAVY_AND_RIGHT_VERTICAL_LIGHT      = SLI("\u253D"); ///< "┽" U+253D
    static constexpr auto RIGHT_HEAVY_AND_LEFT_VERTICAL_LIGHT      = SLI("\u253E"); ///< "┾" U+253E
    static constexpr auto VERTICAL_LIGHT_AND_HORIZONTAL_HEAVY      = SLI("\u253F"); ///< "┿" U+253F
    static constexpr auto UP_HEAVY_AND_DOWN_HORIZONTAL_LIGHT       = SLI("\u2540"); ///< "╀" U+2540
    static constexpr auto DOWN_HEAVY_AND_UP_HORIZONTAL_LIGHT       = SLI("\u2541"); ///< "╁" U+2541
    static constexpr auto VERTICAL_HEAVY_AND_HORIZONTAL_LIGHT      = SLI("\u2542"); ///< "╂" U+2542
    static constexpr auto LEFT_UP_HEAVY_AND_RIGHT_DOWN_LIGHT       = SLI("\u2543"); ///< "╃" U+2543
    static constexpr auto RIGHT_UP_HEAVY_AND_LEFT_DOWN_LIGHT       = SLI("\u2544"); ///< "╄" U+2544
    static constexpr auto LEFT_DOWN_HEAVY_AND_RIGHT_UP_LIGHT       = SLI("\u2545"); ///< "╅" U+2545
    static constexpr auto RIGHT_DOWN_HEAVY_AND_LEFT_UP_LIGHT       = SLI("\u2546"); ///< "╆" U+2546
    static constexpr auto DOWN_LIGHT_AND_UP_HORIZONTAL_HEAVY       = SLI("\u2547"); ///< "╇" U+2547
    static constexpr auto UP_LIGHT_AND_DOWN_HORIZONTAL_HEAVY       = SLI("\u2548"); ///< "╈" U+2548
    static constexpr auto RIGHT_LIGHT_AND_LEFT_VERTICAL_HEAVY      = SLI("\u2549"); ///< "╉" U+2549
    static constexpr auto LEFT_LIGHT_AND_RIGHT_VERTICAL_HEAVY      = SLI("\u254A"); ///< "╊" U+254A
    static constexpr auto HEAVY_VERTICAL_AND_HORIZONTAL            = SLI("\u254B"); ///< "╋" U+254B
    static constexpr auto LIGHT_DOUBLE_DASH_HORIZONTAL             = SLI("\u254C"); ///< "╌" U+254C
    static constexpr auto HEAVY_DOUBLE_DASH_HORIZONTAL             = SLI("\u254D"); ///< "╍" U+254D
    static constexpr auto LIGHT_DOUBLE_DASH_VERTICAL               = SLI("\u254E"); ///< "╎" U+254E
    static constexpr auto HEAVY_DOUBLE_DASH_VERTICAL               = SLI("\u254F"); ///< "╏" U+254F
    static constexpr auto DOUBLE_HORIZONTAL                        = SLI("\u2550"); ///< "═" U+2550
    static constexpr auto DOUBLE_VERTICAL                          = SLI("\u2551"); ///< "║" U+2551
    static constexpr auto DOWN_SINGLE_AND_RIGHT_DOUBLE             = SLI("\u2552"); ///< "╒" U+2552
    static constexpr auto DOWN_DOUBLE_AND_RIGHT_SINGLE             = SLI("\u2553"); ///< "╓" U+2553
    static constexpr auto DOUBLE_DOWN_AND_RIGHT                    = SLI("\u2554"); ///< "╔" U+2554
    static constexpr auto DOWN_SINGLE_AND_LEFT_DOUBLE              = SLI("\u2555"); ///< "╕" U+2555
    static constexpr auto DOWN_DOUBLE_AND_LEFT_SINGLE              = SLI("\u2556"); ///< "╖" U+2556
    static constexpr auto DOUBLE_DOWN_AND_LEFT                     = SLI("\u2557"); ///< "╗" U+2557
    static constexpr auto UP_SINGLE_AND_RIGHT_DOUBLE               = SLI("\u2558"); ///< "╘" U+2558
    static constexpr auto UP_DOUBLE_AND_RIGHT_SINGLE               = SLI("\u2559"); ///< "╙" U+2559
    static constexpr auto DOUBLE_UP_AND_RIGHT                      = SLI("\u255A"); ///< "╚" U+255A
    static constexpr auto UP_SINGLE_AND_LEFT_DOUBLE                = SLI("\u255B"); ///< "╛" U+255B
    static constexpr auto UP_DOUBLE_AND_LEFT_SINGLE                = SLI("\u255C"); ///< "╜" U+255C
    static constexpr auto DOUBLE_UP_AND_LEFT                       = SLI("\u255D"); ///< "╝" U+255D
    static constexpr auto VERTICAL_SINGLE_AND_RIGHT_DOUBLE         = SLI("\u255E"); ///< "╞" U+255E
    static constexpr auto VERTICAL_DOUBLE_AND_RIGHT_SINGLE         = SLI("\u255F"); ///< "╟" U+255F
    static constexpr auto DOUBLE_VERTICAL_AND_RIGHT                = SLI("\u2560"); ///< "╠" U+2560
    static constexpr auto VERTICAL_SINGLE_AND_LEFT_DOUBLE          = SLI("\u2561"); ///< "╡" U+2561
    static constexpr auto VERTICAL_DOUBLE_AND_LEFT_SINGLE          = SLI("\u2562"); ///< "╢" U+2562
    static constexpr auto DOUBLE_VERTICAL_AND_LEFT                 = SLI("\u2563"); ///< "╣" U+2563
    static constexpr auto DOWN_SINGLE_AND_HORIZONTAL_DOUBLE        = SLI("\u2564"); ///< "╤" U+2564
    static constexpr auto DOWN_DOUBLE_AND_HORIZONTAL_SINGLE        = SLI("\u2565"); ///< "╥" U+2565
    static constexpr auto DOUBLE_DOWN_AND_HORIZONTAL               = SLI("\u2566"); ///< "╦" U+2566
    static constexpr auto UP_SINGLE_AND_HORIZONTAL_DOUBLE          = SLI("\u2567"); ///< "╧" U+2567
    static constexpr auto UP_DOUBLE_AND_HORIZONTAL_SINGLE          = SLI("\u2568"); ///< "╨" U+2568
    static constexpr auto DOUBLE_UP_AND_HORIZONTAL                 = SLI("\u2569"); ///< "╩" U+2569
    static constexpr auto VERTICAL_SINGLE_AND_HORIZONTAL_DOUBLE    = SLI("\u256A"); ///< "╪" U+256A
    static constexpr auto VERTICAL_DOUBLE_AND_HORIZONTAL_SINGLE    = SLI("\u256B"); ///< "╫" U+256B
    static constexpr auto DOUBLE_VERTICAL_AND_HORIZONTAL           = SLI("\u256C"); ///< "╬" U+256C
    static constexpr auto LIGHT_ARC_DOWN_AND_RIGHT                 = SLI("\u256D"); ///< "╭" U+256D
    static constexpr auto LIGHT_ARC_DOWN_AND_LEFT                  = SLI("\u256E"); ///< "╮" U+256E
    static constexpr auto LIGHT_ARC_UP_AND_LEFT                    = SLI("\u256F"); ///< "╯" U+256F
    static constexpr auto LIGHT_ARC_UP_AND_RIGHT                   = SLI("\u2570"); ///< "╰" U+2570
    static constexpr auto LIGHT_DIAGONAL_UPPER_RIGHT_TO_LOWER_LEFT = SLI("\u2571"); ///< "╱" U+2571
    static constexpr auto LIGHT_DIAGONAL_UPPER_LEFT_TO_LOWER_RIGHT = SLI("\u2572"); ///< "╲" U+2572
    static constexpr auto LIGHT_DIAGONAL_CROSS                     = SLI("\u2573"); ///< "╳" U+2573
    static constexpr auto LIGHT_LEFT                               = SLI("\u2574"); ///< "╴" U+2574
    static constexpr auto LIGHT_UP                                 = SLI("\u2575"); ///< "╵" U+2575
    static constexpr auto LIGHT_RIGHT                              = SLI("\u2576"); ///< "╶" U+2576
    static constexpr auto LIGHT_DOWN                               = SLI("\u2577"); ///< "╷" U+2577
    static constexpr auto HEAVY_LEFT                               = SLI("\u2578"); ///< "╸" U+2578
    static constexpr auto HEAVY_UP                                 = SLI("\u2579"); ///< "╹" U+2579
    static constexpr auto HEAVY_RIGHT                              = SLI("\u257A"); ///< "╺" U+257A
    static constexpr auto HEAVY_DOWN                               = SLI("\u257B"); ///< "╻" U+257B
    static constexpr auto LIGHT_LEFT_AND_HEAVY_RIGHT               = SLI("\u257C"); ///< "╼" U+257C
    static constexpr auto LIGHT_UP_AND_HEAVY_DOWN                  = SLI("\u257D"); ///< "╽" U+257D
    static constexpr auto HEAVY_LEFT_AND_LIGHT_RIGHT               = SLI("\u257E"); ///< "╾" U+257E
    static constexpr auto HEAVY_UP_AND_LIGHT_DOWN                  = SLI("\u257F"); ///< "╿" U+257F
};

#undef SLI

}

#endif
