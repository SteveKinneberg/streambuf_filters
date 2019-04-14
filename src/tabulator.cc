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

#include <ios_filter/tabulator.h>

namespace ios_filter {
template class basic_tabulator<char>;
template class basic_tabulator<wchar_t>;
template class basic_tabulator<char16_t>;
template class basic_tabulator<char32_t>;
}
