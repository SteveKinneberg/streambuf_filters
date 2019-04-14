#ifndef _utf_utils_h
#define _utf_utils_h
/**
 * @file
 *
 * UTF-8, UTF-16, and UTF-32 utility functions
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

#include <iterator>


namespace ios_filter {
/**
 * Compute the number of bytes/words remaining to complete a Unicode character.
 *
 * UTF-8 characters can be 1, 2, 3, or 4 bytes in size.  UTF-16 can be 1 or 2
 * 16-bit words in size.  UTF-32 characters are only 1 32-bit word in size.
 *
 * If the character only takes 1 bytes/word, then this returns 0 to indicate
 * that no more bytes/words are needed to complete the character.  If the
 * byte/word passed in is the first of a multi- byte/word character, then the
 * number of additional bytes/words needed to complete the character are
 * returned.  All subsequent bytes/words after the first byte/word of a multi-
 * byte/word character will return -1.  This makes it easy to find the boundary
 * of mulit- byte/word characters while iterating in either direction through an
 * array of characters by adding the result of this function to each byte/word
 * in the sequence.  A sum of 0 indicates a complete character.
 *
 * Example: Consider the Euro symbol (€) and the US dollar symbol ($).  In
 * UTF-8, the '€' is a 3 byte character (0xE2, 0x82, 0xAC) while the '$' is a
 * single byte sequence (0x24).  The charts below show the result of calling
 * this function and summing the results of this function as a means to detect
 * the end of a complete character.  In both cases the string will be '$€".
 *
 * Iterate forward:
 *
 * Char | Byte | Return Value | Sum
 * ---- | ---- | ------------ | ---
 * $    | 0x24 |  0           | 0
 * €[0] | 0xE2 |  2           | 2
 * €[1] | 0x82 | -1           | 1
 * €[2] | 0xAC | -1           | 0
 *
 * Iterate backward:
 *
 * Char | Byte | Return Value | Sum
 * ---- | ---- | ------------ | ---
 * €[2] | 0xAC | -1           | -1
 * €[1] | 0x82 | -1           | -2
 * €[0] | 0xE2 |  2           |  0
 * $    | 0x24 |  0           |  0
 *
 * @tparam CharT    The character type.
 *
 * @param ch        The byte/word to evaluate.
 *
 * @return          Number of bytes/words remaining to complete the character.
 */
template <typename CharT>
inline constexpr int utf_char_score(CharT ch [[maybe_unused]]) noexcept
{
    if constexpr (sizeof(CharT) == sizeof(char)) {
        // Multi-byte UTF-8
        if ((ch & 0xC0) == 0x80)     { return -1; }
        if ((ch & 0xE0) == 0xC0)     { return  1; }
        if ((ch & 0xF0) == 0xE0)     { return  2; }
        if ((ch & 0xF8) == 0xF0)     { return  3; }

    } else if constexpr (sizeof(CharT) == sizeof(char16_t)) {
        // Multi-word UTF-16
        if ((ch & 0xFC00) == 0xDC00) { return -1; }
        if ((ch & 0xFC00) == 0xD800) { return  1; }
    }

    // All UTF-32 and single byte/word characters
    return 0;
}


/**
 * This counts Unicode characters in a sequence characters denoted by begin and
 * end iterators.  This is similar to the strlen() C function for purely ASCII
 * strings except that it will count multi-byte characters as a single character
 * rather than the number of bytes like strlen() does.  For example, when CharT
 * = char, then this will return a smaller value than strlen() if the character
 * sequence contains characters like '€'.
 *
 * @tparam Iterator An iterator type that dereferences to a character type.
 *
 * @param begin     The iterator for the beginning of the character sequence.
 * @param end       The iterator for the end of the character sequence.
 *
 * @return          The number of Unicode characters in the sequence.  May be
 *                  less than the number of bytes/words in the sequence.
 */
template <typename Iterator>
std::size_t utflen(Iterator begin, Iterator end) noexcept
{
    using char_type = typename std::iterator_traits<Iterator>::value_type;
    if constexpr (sizeof(char_type) == sizeof (char32_t)) {
        return std::distance(begin, end);
    } else {
        std::size_t c = 0;
        for (auto cbr = 0, it = begin; it != end; ++it) {
            cbr += utf_char_score<char_type>(*it);
            if (cbr == 0) { ++c; }
        }
        return c;
    }
}

/**
 * This overload computes the Unicode character length any kind of ordinary char string.
 *
 * @param s         The string to compute the Unicode character length.
 *
 * @return          The number of Unicode characters in the sequence.  May be
 *                  less than the number of bytes/words in the sequence.
 */
inline std::size_t utflen(std::string_view    s) noexcept { return utflen(s.begin(), s.end()) ; }

/**
 * This overload computes the Unicode character length any kind of wchar_t string.
 *
 * @param s         The string to compute the Unicode character length.
 *
 * @return          The number of Unicode characters in the sequence.  May be
 *                  less than the number of bytes/words in the sequence.
 */
inline std::size_t utflen(std::wstring_view   s) noexcept { return utflen(s.begin(), s.end()) ; }

/**
 * This overload computes the Unicode character length any kind of char16_t string.
 *
 * @param s         The string to compute the Unicode character length.
 *
 * @return          The number of Unicode characters in the sequence.  May be
 *                  less than the number of bytes/words in the sequence.
 */
inline std::size_t utflen(std::u16string_view s) noexcept { return utflen(s.begin(), s.end()) ; }

/**
 * This overload computes the Unicode character length any kind of char32_t string.
 *
 * @param s         The string to compute the Unicode character length.
 *
 * @return          The number of Unicode characters in the sequence.  May be
 *                  less than the number of bytes/words in the sequence.
 */
inline std::size_t utflen(std::u32string_view s) noexcept { return utflen(s.begin(), s.end()) ; }
}

#endif
