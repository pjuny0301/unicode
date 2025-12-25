#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <cstdint>

//AI-generated code (have not been touched by hand)

/* example :
#include "utf/decode.hpp"
#include <iostream>

int main() {
    // 1. u16 -> u8
    std::u16string u16s = u"hello world";
    auto s8 = utf::decode<utf::u8>(u16s); 
    // s8 type : std::basic_string<char8_t> (C++20) or std::string (C++17)

    // 2. u8 -> u16 
    auto s16 = utf::decode<utf::u16>(u8"Hello World");
    
    // 3. u32 -> u8
    auto s32 = utf::decode<utf::u8>(U"😀 Emoji");

    // 4. std::string (char) -> u32
    std::string normal_str = "Standard String";
    auto s32 = utf::decode<utf::u32>(normal_str);

    // 5. decoding same type (soon to be banned)
    auto copy = utf::decode<char>(normal_str);

    return 0;
}
*/
namespace utf {
    // char8_t is standard from C++20, but conditional type definition for C++17 environment
#if defined(__cpp_char8_t)
    using u8 = char8_t;
#else
    using u8 = char; // In C++17 and below, char is treated as u8.
#endif
    using u16 = char16_t;
    using u32 = char32_t;

    constexpr std::uint32_t k_replacement = 0xFFFD;

    // ==========================================
    // Internal Details
    // ==========================================
    namespace detail {
        inline bool is_surrogate(std::uint32_t cp) {
            return (cp >= 0xD800u && cp <= 0xDFFFu);
        }

        // cast char -> u8 
        template<typename T>
        constexpr auto to_u8_val(T c) { return static_cast<std::uint8_t>(c); }

        // --------------------------------------------------
        // Next Codepoint (Decode)
        // --------------------------------------------------
        // UTF-8 (char or char8_t)
        template<typename CharT>
        inline std::uint32_t next_cp_u8(std::basic_string_view<CharT>& s) {
            if (s.empty()) return 0;

            std::uint8_t b0 = to_u8_val(s[0]);
            s.remove_prefix(1);

            if (b0 < 0x80) return b0;

            auto get_cont = [&](std::uint8_t& out) -> bool {
                if (s.empty()) return false;
                std::uint8_t bx = to_u8_val(s[0]);
                if ((bx & 0xC0) != 0x80) return false;
                out = bx;
                s.remove_prefix(1);
                return true;
            };

            std::uint8_t b1{}, b2{}, b3{};

            if (b0 >= 0xC2 && b0 <= 0xDF) {
                if (get_cont(b1)) return ((b0 & 0x1F) << 6) | (b1 & 0x3F);
            } else if (b0 >= 0xE0 && b0 <= 0xEF) {
                if (get_cont(b1) && get_cont(b2)) {
                    std::uint32_t cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
                    if (cp >= 0x800u && !is_surrogate(cp)) return cp;
                }
            } else if (b0 >= 0xF0 && b0 <= 0xF4) {
                if (get_cont(b1) && get_cont(b2) && get_cont(b3)) {
                    std::uint32_t cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
                    if (cp >= 0x10000u && cp <= 0x10FFFFu) return cp;
                }
            }
            return k_replacement;
        }

        // UTF-16
        inline std::uint32_t next_cp_u16(std::basic_string_view<u16>& s) {
            if (s.empty()) return 0;
            std::uint32_t w1 = s[0];
            s.remove_prefix(1);

            if (w1 >= 0xD800u && w1 <= 0xDBFFu) {
                if (s.empty()) return k_replacement;
                std::uint32_t w2 = s[0];
                if (w2 >= 0xDC00u && w2 <= 0xDFFFu) {
                    s.remove_prefix(1);
                    return 0x10000u + (((w1 - 0xD800u) << 10) | (w2 - 0xDC00u));
                }
                return k_replacement;
            }
            if (w1 >= 0xDC00u && w1 <= 0xDFFFu) return k_replacement;
            return w1;
        }

        // UTF-32
        inline std::uint32_t next_cp_u32(std::basic_string_view<u32>& s) {
            if (s.empty()) return 0;
            std::uint32_t cp = static_cast<std::uint32_t>(s[0]);
            s.remove_prefix(1);
            if (cp > 0x10FFFFu || is_surrogate(cp)) return k_replacement;
            return cp;
        }

        // Dispatcher for next_cp
        template<typename CharT>
        inline std::uint32_t next_cp(std::basic_string_view<CharT>& s) {
            if constexpr (sizeof(CharT) == 1) return next_cp_u8(s);
            else if constexpr (sizeof(CharT) == 2) return next_cp_u16(s); // char16_t or wchar_t(win)
            else return next_cp_u32(s);
        }

        // --------------------------------------------------
        // Append Codepoint (Encode)
        // --------------------------------------------------
        template<typename OutStr>
        inline void append_cp(OutStr& out, std::uint32_t cp) {
            using CharT = typename OutStr::value_type;
            if (cp > 0x10FFFFu || is_surrogate(cp)) cp = k_replacement;

            if constexpr (sizeof(CharT) == 1) { // UTF-8
                if (cp <= 0x7Fu) {
                    out.push_back(static_cast<CharT>(cp));
                } else if (cp <= 0x7FFu) {
                    out.push_back(static_cast<CharT>(0xC0u | (cp >> 6)));
                    out.push_back(static_cast<CharT>(0x80u | (cp & 0x3Fu)));
                } else if (cp <= 0xFFFFu) {
                    out.push_back(static_cast<CharT>(0xE0u | (cp >> 12)));
                    out.push_back(static_cast<CharT>(0x80u | ((cp >> 6) & 0x3Fu)));
                    out.push_back(static_cast<CharT>(0x80u | (cp & 0x3Fu)));
                } else {
                    out.push_back(static_cast<CharT>(0xF0u | (cp >> 18)));
                    out.push_back(static_cast<CharT>(0x80u | ((cp >> 12) & 0x3Fu)));
                    out.push_back(static_cast<CharT>(0x80u | ((cp >> 6) & 0x3Fu)));
                    out.push_back(static_cast<CharT>(0x80u | (cp & 0x3Fu)));
                }
            } else if constexpr (sizeof(CharT) == 2) { // UTF-16
                if (cp <= 0xFFFFu) {
                    out.push_back(static_cast<CharT>(cp));
                } else {
                    cp -= 0x10000u;
                    out.push_back(static_cast<CharT>(0xD800u | (cp >> 10)));
                    out.push_back(static_cast<CharT>(0xDC00u | (cp & 0x3FFu)));
                }
            } else { // UTF-32
                out.push_back(static_cast<CharT>(cp));
            }
        }
    } // namespace detail


    // ==========================================
    // Public API: Generic Decode
    // ==========================================

    template<typename ToChar, typename StringT>
    inline std::basic_string<ToChar> decode(const StringT& input) {
        // 1. Unify input to string_view (ViewType)
        using InputChar = std::remove_const_t<std::remove_reference_t<decltype(*std::data(input))>>;
        std::basic_string_view<InputChar> src(std::data(input), std::size(input));

        // 2. when char -> char8_t, u16 -> u16.. etc then deep copy
        if constexpr (sizeof(ToChar) == sizeof(InputChar)) {
            return std::basic_string<ToChar>(reinterpret_cast<const ToChar*>(src.data()), src.size());
        } 
        else {
            // 3. Conversion Logic (Slow Path)
            std::basic_string<ToChar> out;

            // Heuristic scheduling: Prevents unnecessary reallocations
            // - When going to the smaller end: About the original length is sufficient
            // - When going to the larger end: About the original length / 2 (conservative)
 
            if constexpr (sizeof(ToChar) > sizeof(InputChar)) out.reserve(src.size());
            else out.reserve(src.size() * 3 / 2);

            while (!src.empty()) {
                std::uint32_t cp = detail::next_cp(src);
                if (cp == 0) break; // End of string or error handling inside next_cp
                detail::append_cp(out, cp);
            }
            return out;
        }
    }

} // namespace utf
