#include <popcount_ranges/pdep.hpp>

#include <cstdint>
#include <iostream>
#include <limits>
#include <random>

namespace pc = jrmwng::popcount_ranges::stdx;

template <typename T>
constexpr T reference_pdep(T value, T mask) noexcept
{
    T result = 0;
    for (T bit = 1; mask != 0; bit = static_cast<T>(bit << 1))
    {
        if ((mask & bit) != 0)
        {
            if ((value & 1) != 0)
                result |= bit;
            value = static_cast<T>(value >> 1);
            mask &= static_cast<T>(mask - 1);
        }
    }
    return result;
}

static_assert(pc::pdep(std::uint8_t{0b101}, std::uint8_t{0b11010100}) == 0b01000100);
static_assert(pc::pdep(std::uint64_t{0x15}, std::uint64_t{0xF0F0}) == 0x1050);

template <typename T>
bool check_type()
{
    std::mt19937_64 engine(0x5eedu);
    for (std::size_t i = 0; i < 10000; ++i)
    {
        T const value = static_cast<T>(engine());
        T const mask = static_cast<T>(engine());
        auto const expected = reference_pdep(value, mask);
        if (pc::pdep(value, mask) != expected ||
            pc::detail::pdep_software(value, mask) != expected)
            return false;
    }
    constexpr auto max = std::numeric_limits<T>::max();
    return pc::pdep(max, max) == max &&
           pc::pdep(T{0}, max) == T{0} &&
           pc::pdep(max, T{0}) == T{0};
}

int main()
{
    for (unsigned value = 0; value < 256; ++value)
        for (unsigned mask = 0; mask < 256; ++mask)
            if (pc::pdep(static_cast<std::uint8_t>(value),
                         static_cast<std::uint8_t>(mask)) !=
                    reference_pdep(static_cast<std::uint8_t>(value),
                                   static_cast<std::uint8_t>(mask)) ||
                pc::detail::pdep_software(static_cast<std::uint8_t>(value),
                                          static_cast<std::uint8_t>(mask)) !=
                    reference_pdep(static_cast<std::uint8_t>(value),
                                   static_cast<std::uint8_t>(mask)))
            {
                std::cerr << "PDEP mismatch for 8-bit inputs\n";
                return 1;
            }

    if (!check_type<std::uint16_t>() || !check_type<std::uint32_t>() ||
        !check_type<std::uint64_t>())
    {
        std::cerr << "PDEP mismatch for random inputs\n";
        return 1;
    }
    return 0;
}
