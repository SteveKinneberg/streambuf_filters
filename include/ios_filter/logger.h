#ifndef _ios_filter_logger_h
#define _ios_filter_logger_h
/**
 * @file
 *
 * Logger IO stream filter for output streams.
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

#include <any>
#include <chrono>
#include <experimental/source_location>
#include <functional>
#include <numeric>
#include <ostream>
#include <streambuf>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "tabulator.h"


namespace ios_filter
{

/**
 * This class makes formatted logging output easy with C++ I/O streams.
 *
 * Like basic_tabulator, this works by wrapping the output stream's original
 * streambuf and installing itself as the output stream's streambuf.  In fact,
 * it uses basic_tabulator under the covers.
 *
 * This follows the RAII principle in that so long as the instance is in scope,
 * all output will be put into the table.  When the instance is destroyed, the
 * original streambuf will be restored to the output stream and log formatting
 * will end.
 *
 * Generally speaking, it is suggested that this class be instantiated in a
 * private namespace and log taggers be instantiated as globals.  Also, for the
 * sake of uniform output, only use one format for all log taggers.
 *
 * @code{.cpp}
 * static ios_filter::basic_logger<char>::format log_format(logger::timestamp<>{},
 *                                                          logger::tag<3>{},
 *                                                          logger::file<10>{},
 *                                                          logger::line<>{"[", "]"});
 * ios_filter::basic_logger<char> log_entry = log_format.make_log_tagger(std::clog, "tag");
 *
 * void function()
 * {
 *     log_entry() << "Log entry 1";
 *     log_entry() << "Log entry 2" << std::endl;
 *     std::clog << "    continue log entry 2";
 *     log_entry() << "Log entry 3";
 * }
 * @endcode
 *
 * The output of the above sample will look something like:
 * @code{.unparsed}
 * 2019-04-13 17:13:42.441 tag …/sample.cc [21] │ Log entry 1
 * 2019-04-13 17:13:42.442 tag …/sample.cc [21] │ Log entry 2
 *                                              │     continue log entry 2
 * 2019-04-13 17:13:42.444 tag …/sample.cc [21] │ Log entry 3
 * @endcode
 *
 * @tparam CharT    The character type to operate on.
 * @tparam Traits   The character traits to use.
 */
template <typename CharT,
          typename Traits = ::std::char_traits<CharT>>
class basic_logger: public basic_tabulator<CharT, Traits> {
  public:
    /// The character type.
    using char_type           = CharT;
    /// The character traits type.
    using traits_type         = Traits;

    /// Convenience type for the logger type.
    using logger_type         = basic_logger<char_type, traits_type>;

    /// The output stream type.
    using ostream_type        = ::std::basic_ostream<char_type, traits_type>;
    /// The sting type that corresponds to char_type.

    using string_type         = ::std::basic_string<char_type, traits_type>;
    /// The sting_view type that corresponds to char_type.
    using string_view_type    = ::std::basic_string_view<char_type, traits_type>;

    /// The tabulator type.
    using tabulator_type      = basic_tabulator<char_type, traits_type>;
    /// Convenience type for the tabulator cell type.
    using cell_type           = typename tabulator_type::cell_info;
    /// Convenience type for the tabulator cell vector type.
    using cells_type          = typename tabulator_type::cells_type;

    /// Convenience type for location information.
    using location            = std::experimental::source_location;

    /// A callable type that will write something to the log output stream.
    using writer              = ::std::function<void(ostream_type& os)>;
    /**
     * A callable type that takes location information and will write something
     * to the log output stream.
     */
    using loc_writer          = ::std::function<void(ostream_type& os, location const& loc)>;
    /**
     * A callable type that takes tag name information and will write something
     * to the log output stream.
     */
    using tag_writer          = ::std::function<void(ostream_type& os,
                                                     string_view_type tag_name)>;
    /**
     * A callable type that takes user defined information and will write
     * something to the log output stream.
     */
    using user_writer         = ::std::function<void(ostream_type& os, std::any const& user)>;
    /// A variant of all the writer function types.
    using writer_function_var = std::variant<writer, loc_writer, tag_writer, user_writer>;
    /// A collection type for storing all the writer function types.
    using writer_functions    = std::vector<writer_function_var>;

    /**
     * Abstract base class for logging information elements.
     */
    class element_builder {
      public:
        /**
         * Constuctor.
         *
         * @param lpad  Padding to the left of the logging information element.
         * @param rpad  Padding to the right of the logging information element.
         *              Defaults to a single space.
         */
        element_builder(string_view_type lpad = ASCII_DRAWING::NUL,
                        string_view_type rpad = ASCII_DRAWING::SPACE):
            _lpad(lpad),
            _rpad(rpad)
        {}

        /**
         * Move constructor - default implementation.
         *
         * @param other     Other instance to move from.
         */
        element_builder(element_builder&& other) = default;

        /**
         * Move assignment operator - default implementation.
         *
         * @param other     Other instance to move from.
         *
         * @return          Reference to *this.
         */
        element_builder& operator =(element_builder&& other) = default;

        /** Virtual destructor. */
        virtual ~element_builder() {}

        /// @cond DELETED
        element_builder(element_builder const&) = delete;
        element_builder& operator =(element_builder const&) = delete;
        /// @endcond

      protected:
        /**
         * Helper for making a tabulator cell for a logging information element.
         *
         * @param width     Width the logging information element.
         *
         * @return          The newly created tabulator cell.
         */
        cell_type make_cell(std::size_t width) const { return cell_type(width, _lpad, _rpad); }

        /**
         * Derived element builders must implement this method.  It should call
         * the helper version that takes a width parameter.  It may adjust cell
         * formatting before returning the newly created cell.
         *
         * @return          The newly created tabulator cell.
         */
        virtual cell_type make_cell() const = 0;

        /**
         * Derived element builders must implement this method.  It must provide
         * a callable item that must take an output stream as its first
         * parameter and may take any one of tag name string, location, user
         * defined data, or nothing as its second parameter.
         *
         * @return          A logging information element writer callable item.
         */
        virtual writer_function_var make_writer() const = 0;

      private:
        string_view_type _lpad;     ///< Padding to the left of the logging information element.
        string_view_type _rpad;     ///< Padding to the right of the logging information element.

        friend class basic_logger<char_type, traits_type>;
    };

    /**
     * This class builds the timestamp element.  An instance of this will cause
     * a timestamp to be inserted in each log entry.  By default is uses a
     * resolution of 1 millisecond and time's are from the
     * std::chrono::system_clock which uses the UTC time zone.  With C++ 20,
     * std::chrono::local_t may be specified to get timestamps in the local time
     * zone.
     *
     * @tparam Resolution   Timestamp resolution.  Default is std::chrono::milliseconds.
     * @tparam Clock        Clock to use for the timestamps.
     */
    template <typename Resolution = ::std::chrono::milliseconds,
              typename Clock = ::std::chrono::system_clock>
    class timestamp: public element_builder {
      public:
        using element_builder::element_builder;

      protected:
        /**
         * Make the tabulator cell for the timestamp logging information
         * element.
         *
         * @return          The newly created tabulator cell for the timestamp.
         */
        cell_type make_cell() const final
        {
            std::basic_ostringstream<char_type> os;
            auto checker = make_writer();
            std::get<writer>(checker)(os);
            return element_builder::make_cell(utflen(os.str()));
        }

        /**
         * Make the writer for the timestamp logging information element.
         *
         * @return          The timestamp logging information element writer.
         */
        writer_function_var make_writer() const final
        {
            return [](ostream_type& os)
                   {
                       using log_time = ::std::chrono::time_point<Clock, Resolution>;
                       using namespace std::literals::chrono_literals;
                       auto constexpr fmt = "%F %T";
                       log_time now = std::chrono::time_point_cast<Resolution>(Clock::now());
                       std::time_t now_c = Clock::to_time_t(now);
                       os << std::put_time(std::gmtime(&now_c), fmt);
                       if constexpr (Resolution(1) < 1s) {
                           auto since_epoch = now.time_since_epoch();
                           auto frac = since_epoch % 1s;
                           os << '.' << frac.count();
                       }
                   };
        }
    };

    /**
     * This class builds the tag element.  An instance of this will cause a tag
     * name to be inserted in each log entry.  By default, the tag name width is
     * 10 characters.
     *
     * @tparam Width    Width of the tag cell.  Defaults to 10.
     */
    template <std::size_t Width = 10>
    class tag: public element_builder {
      public:
        using element_builder::element_builder;

      protected:
        /**
         * Make the tabulator cell for the tag name logging information element.
         *
         * @return          The newly created tabulator cell for the tag name.
         */
        cell_type make_cell() const final
        {
            return element_builder::make_cell(_width).set_truncate(truncate::right);
        }

        /**
         * Make the writer for the tag name logging information element.
         *
         * @return          The tag name logging information element writer.
         */
        writer_function_var make_writer() const final
        {
            return [] (ostream_type& os, string_view_type tag_name) { os << tag_name; };
        }

      private:
        static constexpr std::size_t _width = Width;    ///< Cell width for the tag name.
    };

    /**
     * This class builds the file name element.  An instance of this will cause
     * a file name to be inserted in each log entry.  By default, the file name
     * width is 32 characters.
     *
     * @tparam Width    Width of the file name cell.  Defaults to 32.
     */
    template <std::size_t Width = 32>
    class file: public element_builder {
      public:
        using element_builder::element_builder;

      protected:
        /**
         * Make the tabulator cell for the file name logging information element.
         *
         * @return          The newly created tabulator cell for the file name.
         */
        cell_type make_cell() const final
        {
            return element_builder::make_cell(_width).set_truncate(truncate::left);
        }

        /**
         * Make the writer for the file name logging information element.
         *
         * @return          The file name logging information element writer.
         */
        writer_function_var make_writer() const final
        {
            return [] (ostream_type& os, location const& loc) { os << loc.file_name(); };
        }

      private:
        static constexpr std::size_t _width = Width;    ///< Cell width for the file name.
    };

    /**
     * This class builds the function name element.  An instance of this will
     * cause a function name to be inserted in each log entry.  By default, the
     * function name width is 32 characters.
     *
     * @tparam Width    Width of the function name cell.  Defaults to 32.
     */
    template <std::size_t Width = 32>
    class function: public element_builder {
      public:
        using element_builder::element_builder;

      protected:
        /**
         * Make the tabulator cell for the function name logging information element.
         *
         * @return          The newly created tabulator cell for the function name.
         */
        cell_type make_cell() const final
        {
            return element_builder::make_cell(_width).set_truncate(truncate::left);
        }

        /**
         * Make the writer for the function name logging information element.
         *
         * @return          The function name logging information element writer.
         */
        writer_function_var make_writer() const final
        {
            return [] (ostream_type& os, location const& loc) { os << loc.function_name(); };
        }

      private:
        static constexpr std::size_t _width = Width;    ///< Cell width for the file name.
    };

    /**
     * This class builds the line number element.  An instance of this will
     * cause a line number to be inserted in each log entry.  By default, the
     * function name width is 4 characters.
     *
     * @tparam Width    Width of the line number cell.  Defaults to 4.
     */
    template <std::size_t Width = 4>
    class line: public element_builder {
      public:
        using element_builder::element_builder;

      protected:
        /**
         * Make the tabulator cell for the line number logging information element.
         *
         * @return          The newly created tabulator cell for the line number.
         */
        cell_type make_cell() const final
        {
            return element_builder::make_cell(_width)
                    .set_justify(justify::right)
                    .set_truncate(truncate::left);
        }

        /**
         * Make the writer for the line number logging information element.
         *
         * @return          The line number logging information element writer.
         */
        writer_function_var make_writer() const final
        {
            return [] (ostream_type& os, location const& loc) { os << loc.line(); };
        }

      private:
        static constexpr std::size_t _width = Width;    ///< Cell width for the file name.
    };

    /**
     * This class builds the user defined data element.  An instance of this will
     * cause the user defined data to be inserted in each log entry.
     *
     * @tparam Width    Width of the line number cell.
     */
    template <std::size_t Width>
    class user: public element_builder {
      public:
        /// Prototype for the user callback.

        /**
         * Constructor.
         *
         * @param callback  Function that formats the supplied data for the output stream.
         * @param lpad      Padding to the left of the logging information element.
         * @param rpad      Padding to the right of the logging information element.
         *                  Defaults to a single space.
         */
        user(user_writer callback,
             string_view_type lpad = ASCII_DRAWING::NUL,
             string_view_type rpad = ASCII_DRAWING::SPACE):
            element_builder(lpad, rpad),
            _callback(callback)
        {}

      protected:
        /**
         * Make the tabulator cell for the line number logging information element.
         *
         * @return          The newly created tabulator cell for the line number.
         */
        cell_type make_cell() const final
        {
            return element_builder::make_cell(_width).set_truncate(truncate::right);
        }

        /**
         * Make the writer for the line number logging information element.
         *
         * @return          The line number logging information element writer.
         */
        writer_function_var make_writer() const final
        {
            return _callback;
        }

      private:
        static constexpr std::size_t _width = Width;    ///< Cell width for the file name.
        user_writer _callback;
    };

    /**
     * This class is use for specifying the logging format.  Generally, there
     * should only be one instance of this class in a given project to ensure
     * logging uniformity.
     */
    class format
    {
      public:
        /**
         * Constructor.
         *
         * @tparam Args     Types for the parameter pack.
         *
         * @param args      Parameter pack of the logging information element builders.
         */
        template <typename... Args>
        format(Args&&... args) { process_fmt_args(std::forward<Args>(args)...); }

        /**
         * Factory method that creates a logger for the given output stream.
         *
         * @param os        Output stream to apply logger formatting to.
         * @param tag_name  Tag name for this logger being created.
         *
         * @return          A new logger instance using this format.
         */
        logger_type make_log_tagger(ostream_type& os, string_type tag_name = ASCII_DRAWING::NUL)
        {
            return logger_type(os, _cells, _writer_funcs, tag_name);
        }

      private:
        cells_type _cells;                  ///< The logging information element cells.
        writer_functions _writer_funcs;     ///< The logging information element writers.

        /**
         * Adds a static string to all loggers based off this format.
         *
         * @param arg   The string to add to logging output.
         */
        void process_fmt_arg(string_view_type arg)
        {
            _cells.emplace_back(cell_type{ arg.size(),
                                           ASCII_DRAWING::NUL,
                                           ASCII_DRAWING::NUL,
                                           ASCII_DRAWING::NUL });
            _writer_funcs.emplace_back([arg] (ostream_type& os) { os << arg; });
        }

        /**
         * Add a logging information element builder.
         *
         * @param arg   The logging information element to add to logging output.
         */
        void process_fmt_arg(element_builder const& builder)
        {
            _cells.emplace_back(std::move(builder.make_cell()));
            _writer_funcs.emplace_back(std::move(builder.make_writer()));
        }

        /**
         * Method for recursively unpacking the parameter pack passed to the
         * constructor.
         *
         * @tparam T    The type for the current argument being processed.
         * @tparam Ts   The types for the remaining argument pack elements.
         *
         * @param arg   The current argument to be processed.
         * @param args  Remaining argument pack to be processed.
         */
        template <typename T, typename... Ts>
        void process_fmt_args(T&& arg, Ts... args)
        {
            process_fmt_arg(std::forward<T>(arg));
            if constexpr (sizeof...(args) > 0) process_fmt_args(std::forward<Ts>(args)...);
        }
    };

    /// Destructor.
    ~basic_logger() { go_first_column(); }

    /**
     * This starts a new log entry.  This version takes a user defined value
     * as an argument.  Use this form if the logger format includes a user
     * callback function.
     *
     * @param data  The user defined parameter.
     * @param loc   The location information to include in the log entry.
     *              Defaults to the location where this is called.
     *
     * @return      A reference to the output stream.
     */
    ostream_type& operator ()(std::any const& data, location loc = location::current())
    {
        go_first_column();

        tabulator_type entry_info(_os, _cells);
        entry_info.set_style(tabulator_type::empty);

        for (auto const& fn: _functions)
        {
            std::visit([data, loc, &tag = _tag_name, &os = _os](auto&& arg) {
                           using T = std::decay_t<decltype(arg)>;
                           if constexpr (std::is_same_v<T, writer>)      { arg(os); }
                           if constexpr (std::is_same_v<T, loc_writer>)  { arg(os, loc); }
                           if constexpr (std::is_same_v<T, tag_writer>)  { arg(os, tag); }
                           if constexpr (std::is_same_v<T, user_writer>) { arg(os, data); }
                       }, fn);
            _os << endc;
        }
        next_column();
        return _os;
    }

    /**
     * This starts a new log entry.  Use this form if the logger format does
     * not include a user callback function.
     *
     * @param loc   The location information to include in the log entry.
     *              Defaults to the location where this is called.
     *
     * @return      A reference to the output stream.
     */
    ostream_type& operator ()(location loc = location::current())
    {
        return (*this)(std::any(),loc);
    }

  private:
    using tabulator_type::get_current_column;
    using tabulator_type::next_column;
    using tabulator_type::set_style;
    using tabulator_type::_os;
    using typename tabulator_type::ASCII_DRAWING;

    cells_type _cells;              ///< The cell information for each log information element.
    writer_functions _functions;    ///< The set of log writer functions to call on each new entry.
    string_type const _tag_name;    ///< The tag name to include in the log entry information.

    /**
     * Constructor for log entries.  Can only be created by basic_logger::make_log_tagger().
     *
     * @param os            Reference to the output stream to wrap.
     * @param cells         The @ref basic_tabulator cell definitions.
     * @param functions     The log element functions to call.
     * @param tag_name           Tag name for the log entry.
     */
    basic_logger(ostream_type& os,
                 cells_type const& cells,
                 writer_functions functions,
                 string_type& tag_name):
        tabulator_type(os, { { std::accumulate(cells.begin(), cells.end(), std::size_t(0),
                                               [] (std::size_t const& sum, cell_type const& cell)
                                               { return sum + cell.get_cell_width(); }),
                               ASCII_DRAWING::NUL, ASCII_DRAWING::SPACE, ASCII_DRAWING::NUL },
                             { 0, ASCII_DRAWING::SPACE, ASCII_DRAWING::NUL, ASCII_DRAWING::NUL } }),
        _cells(cells),
        _functions(functions),
        _tag_name(tag_name)
    {
        assert(cells.size() == functions.size());
        set_style(tabulator_type::borderless_box);
    }

    /**
     * Advance the columns until it wraps back to the first column.
     */
    void go_first_column()
    {
        while (get_current_column() != 0) { next_column(); }
    }
};

extern template class basic_logger<char>;
extern template class basic_logger<wchar_t>;
extern template class basic_logger<char16_t>;
extern template class basic_logger<char32_t>;

/// Type alias for a logger filter using ordinary chars.
using logger    = basic_logger<char>;

/// Type alias for a logger filter using wchar_t.
using wlogger   = basic_logger<wchar_t>;

/// Type alias for a logger filter using char16_t.
using u16logger = basic_logger<char16_t>;

/// Type alias for a logger filter using char32_t.
using u32logger = basic_logger<char32_t>;

}

#endif
