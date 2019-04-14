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

#include <chrono>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <thread>
#include "ios_filter/logger.h"

using namespace ios_filter;

struct foo {
    std::string value;
};

static logger::format log_format(logger::timestamp<>{}, ": ",
                                 logger::tag<>{},
                                 logger::function<10>{}, logger::file<>{}, logger::line<>{"[", "] "},
                                 logger::user<10>{[] (std::ostream& os, std::any const& data)
                                                  {
                                                      if (foo const* f= std::any_cast<foo>(&data))
                                                      {
                                                          os << f->value;
                                                      }
                                                  }
                                             }
                                 );
auto log_entry = log_format.make_log_tagger(std::clog, "sample");


int main()
{
    using namespace std::chrono_literals;

    foo f1;
    f1.value = "hello";

    foo f2;
    f2.value = "world";

    log_entry(f1) << "Log line 1";
    log_entry(f2) << "Log line 2 start:" << std::flush;
    std::this_thread::sleep_for(1s);
    std::clog << "stop";
    log_entry(f1) << "Log line 3" << std::endl;
    std::clog << "Log line 4";
    log_entry(123) << "Log line 5";

    return 0;
}
