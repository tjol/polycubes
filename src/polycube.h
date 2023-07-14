#ifndef POLYCUBES_POLYCUBE_H_
#define POLYCUBES_POLYCUBE_H_

#include "coord.h"

#include <array>
#include <algorithm>
#include <iostream>
#include <span>

template <size_t SIZE>
int coord_cmp(std::span<Coord const, SIZE> aa, std::span<Coord const, SIZE> bb)
{
    for (size_t i{}; i < SIZE; ++i) {
        switch (coord_cmp(aa[i], bb[i])) {
        case -1: return -1;
        case 1: return 1;
        default: break;
        }
    }
    return 0;
}

template <size_t SIZE>
int coord_cmp(std::array<Coord, SIZE> const& aa, std::array<Coord, SIZE> const& bb)
{
    return coord_cmp(std::span{aa}, std::span{bb});
}

template <size_t SIZE>
struct PolyCube
{
    static size_t constexpr cube_count = SIZE;

    std::array<Coord, SIZE> cubes;

    PolyCube() {}

    template<IsCoord ... CoordType>
    PolyCube(CoordType ... block) : cubes{block...} {}


    // rotate the shape and perform partial normalization
    PolyCube<SIZE> rot(int orientation) const {
        PolyCube<SIZE> result;
        // rotate all the coordinates individually
        std::transform(cubes.cbegin(), cubes.cend(), result.cubes.begin(),
            [orientation](Coord const& p) { return p.rot(orientation); });
        // make the minimum Coord 0 in all dimensions
        auto origin = min_coords(result.cubes);
        for (auto& block : result.cubes) {
            block -= origin;
        }
        // sort the coordinates (the order doesn't matter - sorted is defined as
        // the normal form)
        std::sort(result.cubes.begin(), result.cubes.end(),
            [](auto const& a, auto const& b) { return coord_cmp(a, b) == -1; });
        return result;
    }

    // get all orientations of this shape
    std::array<PolyCube<SIZE>, N_ROTATIONS> all_rots() const {
        std::array<PolyCube<SIZE>, N_ROTATIONS> result{};
        for (int i = 0; i < N_ROTATIONS; ++i) {
            result[i] = rot(i);
        }
        return result;
    }

    // get the normal form (orientation/representation) of this shape
    PolyCube<SIZE> normal() const {
        // the coordinate representations of the shape have an absolute order
        // the "minimum" is defined as the normal form
        auto tutti = all_rots();
        return *std::min_element(tutti.cbegin(), tutti.cend(),
            [](auto const& a, auto const& b) { return coord_cmp(a.cubes, b.cubes) == -1; });
    }

};

template <size_t SIZE>
std::ostream& operator<<(std::ostream& os, PolyCube<SIZE> const& s)
{
    os << "[[ ";
    for (auto const& p : s.cubes) {
        os << p << ' ';
    }
    os << "]]";
    return os;
}

template <size_t SIZE>
bool operator==(PolyCube<SIZE> const& a, PolyCube<SIZE> const& b)
{
    for (size_t i{}; i < SIZE; ++i)
        if (a.cubes[i] != b.cubes[i]) return false;
    return true;
}

template<typename T, typename=void> bool constexpr is_polycube = false;
template<size_t SIZE> bool constexpr is_polycube<PolyCube<SIZE>> = true;
template<typename T> concept PolyCuboid = is_polycube<T>;
template<typename T> bool constexpr is_polycube_deref = is_polycube<std::remove_cvref_t<T>>;
template<typename T> concept PolyCuboidDeref = is_polycube_deref<T>;

namespace std {
    template<PolyCuboid PolyCubeType>
    struct hash<PolyCubeType>
    {
        size_t operator()(PolyCubeType const &s) const
        {
            size_t hash{};
            for (size_t i{}; i < PolyCubeType::cube_count; ++i)
                hash ^= std::hash<Coord>{}(s.cubes[i]) << i;
            return hash;
        }
    };
}

#endif // POLYCUBES_POLYCUBE_H_
