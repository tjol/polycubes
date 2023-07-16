#ifndef POLYCUBES_UTIL_H_
#define POLYCUBES_UTIL_H_

#include <concepts>
#include <format>

template <std::integral T, T MAX, template<T i> class F>
struct metaswitch
{
    template <typename ... Args>
    auto operator()(T v, Args&& ... args)
    {
        if (v == MAX) {
            return F<MAX>{}(std::forward<Args>(args)...);
        } else if constexpr (MAX == 0) {
            throw std::runtime_error(std::format("invalid value: {}", v));
        } else {
            return metaswitch<T, MAX - 1, F>{}(v, std::forward<Args>(args)...);
        }
    }
};

template<typename R1, typename R2, typename OutFunc>
void merge_uniq(R1 old_range, std::span<R2> new_ranges, OutFunc f)
{
    using Iter1 = std::remove_cvref_t<decltype(std::begin(old_range))>;
    using Iter2 = std::remove_cvref_t<decltype(std::begin(new_ranges[0]))>;
    using Value = std::remove_cvref_t<decltype(*std::declval<Iter1>())>;

    auto iter1 = std::begin(old_range);
    auto iters2 = std::vector<Iter2>(new_ranges.size());
    std::transform(std::begin(new_ranges), std::end(new_ranges), std::begin(iters2),
        [](auto& r) { return std::begin(r); });

    auto end1 = std::end(old_range);
    auto ends2 = std::vector<Iter2>(new_ranges.size());
    std::transform(std::begin(new_ranges), std::end(new_ranges), std::begin(ends2),
        [](auto& r) { return std::end(r); });

    for (;;) {
        Value const* minval = nullptr;
        int min_idx = -1;
        if (iter1 != end1) minval = &(*iter1);
        for (int i = 0; i < (int)iters2.size(); ++i) {
            if (iters2[i] != ends2[i]) {
                auto const& val = *iters2[i];
                if (minval == nullptr || val < *minval) {
                    // this might be the next value
                    min_idx = i;
                    minval = &val;
                } else if (val == *minval) {
                    // this is a duplicate, skip it
                    ++iters2[i];
                }
            }
        }

        if (minval == nullptr) break; // eof

        f(*minval);
        if (min_idx == -1) {
            ++iter1;
        } else {
            ++iters2[min_idx];
        }
    }
}


#endif // POLYCUBES_UTIL_H_
