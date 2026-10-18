#include <popcount_ranges/set.hpp>

#include <cstdint>

int main()
{
    using two_bit_values = jrmwng::popcount_ranges::set<std::uint8_t{2}>;
    two_bit_values values;
    return values.size() == 28 ? 0 : 1;
}
