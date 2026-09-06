#include "simd.hpp"

#include <bitset>
#include <tuple>
#include <cstdint>

namespace jrmwng
{
    namespace data_par
    {
        template <typename T, typename Tsimd_abi = typename simd<T>::abi_type>
        struct simd_range_traits
        {
            using simd_type = basic_simd<T, Tsimd_abi>;
            using mask_type = basic_simd_mask<sizeof(T), Tsimd_abi>;
            inline constexpr static std::size_t simd_width = simd_type::size();

            using bitset_type = std::bitset<simd_width>;

            template <typename U = T>
            constexpr static U * align_floor(U *ptr)
            {
                static_assert(std::is_same_v<std::remove_cv_t<U>, T>, "Pointer type must match SIMD element type.");
                auto addr = reinterpret_cast<std::uintptr_t>(ptr);
                addr -= addr % (simd_width * sizeof(T));
                return reinterpret_cast<U *>(addr);
            }
            template <typename U = T>
            constexpr static U * align_ceil(U *ptr)
            {
                static_assert(std::is_same_v<std::remove_cv_t<U>, T>, "Pointer type must match SIMD element type.");
                auto addr = reinterpret_cast<std::uintptr_t>(ptr);
                addr -= addr % (simd_width * sizeof(T));
                addr += (addr == reinterpret_cast<std::uintptr_t>(ptr)) ? 0 : (simd_width * sizeof(T));
                return reinterpret_cast<U *>(addr);
            }

            static constexpr bitset_type make_bitset(size_t const start, size_t const count)
            {
                return bitset_type{}.set() >> (simd_width - count) << start;
            }
            static constexpr mask_type make_mask(bitset_type const &bits)
            {
                if constexpr (std::constructible_from<mask_type, bitset_type>)
                {
                    return mask_type(bits);
                }
                else
                {
                    return mask_type([bits](auto uIndex)
                    {
                        return bits.test(uIndex);
                    });
                }
            }
            
            // Create a mask with bits set from 'offset' to end (upper bits)
            // Example: offset=3, width=8 → 0b11111000 (bits 3-7 set)
            static constexpr mask_type make_head_mask(size_t const valid_count)
            {
                // Verify offset is in valid range [0, simd_width)
                // When uIndex >= offset, bits [offset, simd_width) are set
                if constexpr (std::constructible_from<mask_type, bitset_type>)
                {
                    return mask_type(bitset_type().set() << (simd_width - valid_count));
                }
                else
                {
                    return mask_type([valid_count](auto uIndex)
                    {
                        return uIndex >= (simd_width - valid_count);
                    });
                }
            }
            static constexpr mask_type make_head_mask(T const *ptr)
            {
                return make_head_mask(simd_width - (ptr - align_floor(ptr)));
            }
            
            // Create a tail mask where 'valid_count' is the number of valid elements from the start of the SIMD chunk
            // Example: end at offset 3, width=8, count=5 → 0b00000111 (bits 0-2 set, masking 3 valid elements)
            // The mask sets the lower (simd_width - count) bits, excluding the upper 'count' padding elements
            static constexpr mask_type make_tail_mask(size_t const valid_count)
            {
                // valid_count = number of valid elements from the start of the SIMD chunk
                // Padding elements = simd_width - valid_count
                // Set bits [0, valid_count) by testing: uIndex < valid_count
                if constexpr (std::constructible_from<mask_type, bitset_type>)
                {
                    return mask_type(bitset_type().set() >> (simd_width - valid_count));
                }
                else
                {
                    return mask_type([valid_count](auto uIndex)
                    {
                        return uIndex < valid_count;
                    });
                }
            }
            static constexpr mask_type make_tail_mask(T const *ptr)
            {
                return make_tail_mask(simd_width - (align_ceil(ptr) - ptr));
            }
        };

        // SIMD range iterator for contiguous data
        // 
        // This iterator processes contiguous ranges using SIMD loads with appropriate masking.
        // It handles three cases:
        // 
        // 1. Multiple aligned loads: Range spans multiple SIMD-width chunks
        //    - First chunk: aligned load from align_floor(begin), uses head_mask to exclude elements before range start
        //    - Middle chunks: aligned loads, use all-true mask (all elements valid)
        //    - Last chunk: aligned load, uses tail_mask to exclude elements after range end
        //    - Load size: Full simd_width for all chunks (aligned loads)
        // 
        // 2. Single aligned load: Range fits within one SIMD-width aligned chunk (PREFERRED for small ranges)
        //    - Combines head_mask & tail_mask to exclude elements before start AND after end
        //    - Load size: popcount(combined_mask) - only loads valid elements to avoid reading beyond range
        //    - This is prioritized over unaligned loads to prevent page boundary violations
        //    - Triggered when: align_floor(begin) + simd_width == align_ceil(end)
        // 
        // 3. Single unaligned load: Small range that spans alignment boundary
        //    - Loads directly from range start (unaligned) with size-based mask
        //    - Load size: popcount(size_mask) - limited to range size to minimize unaligned access
        //    - Only used when: size < simd_width AND range doesn't fit in single aligned chunk
        //    - Example: 3 elements starting at offset 7 with simd_width=8 (crosses to next chunk)
        // 
        // Mask semantics:
        // - head_mask: For first aligned load, masks out elements BEFORE range start
        //   Created by make_head_mask(begin_ptr): sets bits [offset, simd_width) 
        //   where offset = begin_ptr - align_floor(begin_ptr)
        //   Example: begin at offset 2, width=8 → bits 2-7 set (0b11111100)
        // 
        // - tail_mask: For last aligned load, masks out elements AFTER range end  
        //   Created by make_tail_mask(end_ptr): sets bits [0, simd_width - count) 
        //   where count = align_ceil(end_ptr) - end_ptr (padding after range end)
        //   Example: end at offset 6, width=8, align_ceil=8 → count=2, bits 0-5 set (0b00111111)
        // 
        // - combined_mask: For single aligned load, head_mask & tail_mask
        //   Masks out elements both before start AND after end
        //   Example: offset 2, end 6, width=8 → bits 2-5 set (0b00111100) = 4 valid elements
        //   Load uses popcount(combined_mask) = 4, not full simd_width
        template <typename T, typename Tsimd_abi = typename simd<T>::abi_type>
            requires std::is_arithmetic_v<T>
        class simd_range
        {
            using traits = simd_range_traits<T, Tsimd_abi>;
            
            using iterator_tuple_type = std::tuple<T const *, typename traits::mask_type, T const *, typename traits::mask_type>;
            iterator_tuple_type m_tupleIterator; // align_floor(begin()), head_mask, align_ceil(end()), tail_mask

        public:
            using simd_type = typename traits::simd_type;
            using mask_type = typename traits::mask_type;
            
            class iterator
            {
                iterator_tuple_type m_tupleIterator; // align_floor(current()), current_mask, align_ceil(end()), tail_mask

            public:
                using difference_type = std::ptrdiff_t;
                using value_type = std::pair<simd_type, mask_type>;
                
                iterator(iterator_tuple_type const& tupleIterator)
                    : m_tupleIterator(tupleIterator)
                {}
                
                value_type operator*() const
                {
                    auto const [current, current_mask, end, tail_mask] = m_tupleIterator;
                    auto const next = current + traits::simd_width;

                    if (next != end)
                    {
                        // Not the last chunk: aligned load with current_mask
                        // - First iteration: uses head_mask to exclude elements before range start
                        // - Subsequent iterations: uses all-true mask (set by operator++)
                        // - Always loads full simd_width from aligned address
                        return value_type{unchecked_load<simd_type>(std::get<0>(m_tupleIterator), traits::simd_width, flag_aligned), current_mask};
                    }
                    else
                    {
                        // Last chunk (next == end):
                        // All three cases load full simd_width and rely on tail_mask for filtering:
                        //
                        // Case 1: Last of multiple aligned loads
                        //   - current = last aligned chunk address
                        //   - tail_mask excludes padding elements after range end
                        //
                        // Case 2: Single aligned load (after head_mask & tail_mask combination in constructor)
                        //   - current = align_floor(begin)
                        //   - tail_mask = combined mask excluding padding before start AND after end
                        //   - IMPORTANT: Loads full simd_width, mask filters invalid elements
                        //   - This is why we use simd_width, not popcount(tail_mask)!
                        //
                        // Case 3: Single unaligned load (for small cross-alignment ranges)
                        //   - current = actual range start (unaligned)
                        //   - tail_mask = size-based mask (bits [0, size) set)
                        //   - Loads full simd_width from unaligned address
                        //
                        // flag_default used for all cases because:
                        // - Cases 1&2: current may or may not be aligned depending on page layout
                        // - Case 3: explicitly unaligned
                        // - Performance: unaligned load with full simd_width + mask filtering is
                        //   often faster than partial load with temporary buffer
                        return value_type{unchecked_load<simd_type>(std::get<0>(m_tupleIterator), traits::simd_width, flag_default), tail_mask};
                    }
                }
                
                iterator& operator++()
                {
                    auto const [current, current_mask, end, tail_mask] = m_tupleIterator;
                    auto const next = current + traits::simd_width;

                    std::get<0>(m_tupleIterator) = next;
                    std::get<1>(m_tupleIterator) = mask_type(true); // Set all bits to 1
                    return *this;
                }
                
                iterator operator++(int)
                {
                    iterator tmp = *this;
                    ++(*this);
                    return tmp;
                }
                
                bool operator==(iterator const& other) const
                {
                    return std::get<0>(m_tupleIterator) == std::get<0>(other.m_tupleIterator);
                }
                
                bool operator!=(iterator const& other) const
                {
                    return !(*this == other);
                }
            };
            
            template <std::ranges::contiguous_range R>
                requires std::is_arithmetic_v<T> && std::same_as<std::ranges::range_value_t<R>, T>
            simd_range(R && data)
                : m_tupleIterator
                (
                    traits::align_floor(std::ranges::data(data)),
                    traits::make_head_mask(std::ranges::data(data)),
                    traits::align_ceil(std::ranges::data(data) + std::ranges::size(data)),
                    traits::make_tail_mask(std::ranges::data(data) + std::ranges::size(data))
                )
            {
                auto &[current, current_mask, end, tail_mask] = m_tupleIterator;
                
                // Priority 1: Single aligned load (HIGHEST PRIORITY to avoid page boundary violations)
                // When the range fits within one aligned SIMD chunk, combine head and tail masks
                // This is preferred over unaligned load even for small ranges when alignment allows it
                if (current + traits::simd_width == end)
                {
                    tail_mask &= current_mask;
                    current_mask = mask_type(false);
                }
                // Priority 2: Single unaligned load for small ranges that don't fit in single aligned chunk
                // This handles ranges where size < simd_width AND the range spans across alignment boundary
                // The unaligned load is necessary but limited to popcount() elements to minimize risk
                else if (0 < std::ranges::size(data) && std::ranges::size(data) < traits::simd_width)
                {
                    std::get<0>(m_tupleIterator) = std::ranges::data(data);
                    std::get<1>(m_tupleIterator) = mask_type(false);
                    std::get<2>(m_tupleIterator) = std::ranges::data(data) + traits::simd_width;
                    std::get<3>(m_tupleIterator) = mask_type([count= std::ranges::size(data)](auto uIndex)
                    {
                        return uIndex < count;
                    });
                }
                // Default: Multiple aligned loads (no special handling needed here)
            }
            
            iterator begin() const
            {
                return iterator(m_tupleIterator);
            }
            
            iterator end() const
            {
                return iterator(iterator_tuple_type(std::get<2>(m_tupleIterator), mask_type(false), std::get<2>(m_tupleIterator), std::get<3>(m_tupleIterator)));
            }
        };

        // For long range, we use `simd_range` to perform vector-aligned loads
        // For short range, we use `simd_short_range` to perform element-wise aligned loads
        template <typename T, typename Tsimd_abi = typename simd<T>::abi_type>
            requires std::is_arithmetic_v<T>
        class simd_short_range
        {
            using traits = simd_range_traits<T, Tsimd_abi>;
            using bitset_type = typename traits::bitset_type;

            std::tuple<T const *, T const *, bitset_type, size_t> m_tuple; // (first, last, mask, length)

            static auto prepare(T const *m_first, T const *m_last) noexcept
            {
                size_t const count = (m_last - m_first) % traits::simd_width;
                size_t const valid_count = count ? count : traits::simd_width;

                auto const aligned_first = traits::align_floor(m_first);
                bool const single_vector_case = (aligned_first == traits::align_floor(m_last - 1)); // all elements fit into the same SIMD vector

                auto first = m_first;
                auto last = m_last;
                typename traits::bitset_type mask{};
                size_t length{};
                if (m_first < m_last)
                {
                    first = (single_vector_case ? aligned_first : m_first);
                    mask = traits::make_bitset(m_first - first, valid_count);
                    last = (single_vector_case ? aligned_first + valid_count : m_last);
                    length = (m_last - m_first + traits::simd_width - 1u) / traits::simd_width;
                }
                return std::tuple{first, last, mask, length};
            }
        public:
            using simd_type = typename traits::simd_type;
            using mask_type = typename traits::mask_type;

            using value_type = std::tuple<simd_type, mask_type>;

            simd_short_range(T const * first, T const * last) noexcept
                : m_tuple(prepare(first, last))
            {
            }

            struct iterator
            {
                using value_type = std::tuple<simd_type, mask_type>;
                using iterator_category = std::input_iterator_tag;
                using difference_type = std::ptrdiff_t;

                T const * m_current{};
                bitset_type m_mask{};

                bool operator==(const iterator& other) const noexcept { return m_current == other.m_current; }
                bool operator!=(const iterator& other) const noexcept { return m_current != other.m_current; }

                // Load a SIMD vector
                value_type operator*() const
                {
                    return value_type(unchecked_load<simd_type>(m_current, traits::simd_width, flag_default), traits::make_mask(m_mask));
                }
                // Advance to the next SIMD vector for continuous memory reading
                iterator& operator++() noexcept
                {
                    // `popcount(m_mask)` is used here for friendliness of compiler optimization.
                    // It requires both `begin()` method and `end()` method to handle this properly.
                    m_current += m_mask.count();

                    // By design, only the first iteration could be partial load.
                    // All subsequent iterations are full vector loads.
                    // This enables compiler inferring `m_mask == mask_type(true)` for subsequent iterations.
                    m_mask.set();
                    return *this;
                }
                iterator operator++(int) noexcept
                {
                    iterator tmp = *this;
                    ++(*this);
                    return tmp;
                }
            };
            iterator begin() const noexcept
            {
                return iterator{std::get<0>(m_tuple), std::get<2>(m_tuple)};
            }

            iterator end() const noexcept
            {
                return iterator{std::get<1>(m_tuple), bitset_type{}};
            }

            size_t size() const noexcept
            {
                return std::get<3>(m_tuple);
            }
            value_type operator[] (size_t index) const noexcept
            {
                if (index == 0)
                    return value_type(unchecked_load<simd_type>(std::get<0>(m_tuple), traits::simd_width, flag_default), traits::make_mask(std::get<2>(m_tuple)));
                else if (index < size())
                    return value_type(unchecked_load<simd_type>((std::get<0>(m_tuple) + std::get<2>(m_tuple).count() - traits::simd_width) + index * traits::simd_width, traits::simd_width, flag_default), mask_type(true));
                else
                    return value_type(simd_type{}, mask_type(false));
            }
        };
    }
}