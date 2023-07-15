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

template <typename Iter1, typename Iter2, typename OutFunc>
void merge_uniq(Iter1 begin1, Iter1 end1, Iter2 begin2, Iter2 end2, OutFunc f)
{
    auto iter1 = begin1;
    auto iter2 = begin2;

    while (iter1 != end1 && iter2 != end2) {
        if (*iter1 == *iter2) {
            ++iter2;
        } else if (*iter1 < *iter2) {
            f(*iter1);
            ++iter1;
        } else {
            f(*iter2);
            ++iter2;
        }
    }

    while (iter1 != end1) {
        f(*iter1);
        ++iter1;
    }

    while (iter2 != end2) {
        f(*iter2);
        ++iter2;
    }
}


#endif // POLYCUBES_UTIL_H_
