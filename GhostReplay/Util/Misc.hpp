#pragma once
#include <type_traits>

template <typename Enumeration>
constexpr std::enable_if_t<std::is_enum<Enumeration>::value,
    std::underlying_type_t<Enumeration>> as_int(const Enumeration value) {
    return static_cast<std::underlying_type_t<Enumeration>>(value);
}
