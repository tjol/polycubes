#ifndef POLYCUBES_UTIL_H_
#define POLYCUBES_UTIL_H_

#include <concepts>

template <std::integral T, T MAX, template<T i> class F>
struct metaswitch
{
    template <typename ... Args>
    auto operator()(T v, Args&& ... args)
    {
        if (v == MAX) {
            return F<MAX>{}(std::forward<Args>(args)...);
        } else if constexpr (MAX == 0) {
            throw std::runtime_error("invalid value in metaswitch");
        } else {
            return metaswitch<T, MAX - 1, F>{}(v, std::forward<Args>(args)...);
        }
    }
};



#endif // POLYCUBES_UTIL_H_