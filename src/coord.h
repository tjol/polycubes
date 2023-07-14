#ifndef POLYCUBES_COORD_H_
#define POLYCUBES_COORD_H_

#include <array>
#include <stdexcept>
#include <iostream>
#include <limits>
#include <functional>
#include <stdint.h>

int constexpr N_ROTATIONS = 24;

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#endif

struct Coord {
    using Scalar = int8_t;

    std::array<Scalar, 3> xyz;

    Coord(Scalar x, Scalar y, Scalar z) : xyz{x, y, z} {}
    Coord() {}

    Coord rot(int orientation) const
    {
        auto const& [x, y, z] = xyz;
        switch (orientation)
        {
            case 0: return {x, y, z};
            case 1: return {y, -x, z};
            case 2: return {-x, -y, z};
            case 3: return {-y, x, z};
            case 4: return {z, y, -x};
            case 5: return {y, -z, -x};
            case 6: return {-z, -y, -x};
            case 7: return {-y, z, -x};
            case 8: return {-x, y, -z};
            case 9: return {y, x, -z};
            case 10: return {x, -y, -z};
            case 11: return {-y, -x, -z};
            case 12: return {-z, y, x};
            case 13: return {y, z, x};
            case 14: return {z, -y, x};
            case 15: return {-y, -z, x};
            case 16: return {x, z, -y};
            case 17: return {z, -x, -y};
            case 18: return {-x, -z, -y};
            case 19: return {-z, x, -y};
            case 20: return {-x, z, y};
            case 21: return {z, x, y};
            case 22: return {x, -z, y};
            case 23: return {-z, -x, y};
            default: throw std::invalid_argument("orientation must be [0..23]");
        }
    }

    Scalar x() const { return xyz[0]; }
    Scalar y() const { return xyz[1]; }
    Scalar z() const { return xyz[2]; }

};

inline Coord operator+(Coord const& a, Coord const& b)
{
    return {a.x() + b.x(), a.y() + b.y(), a.z() + b.z()};
}

inline Coord operator-(Coord const& a, Coord const& b)
{
    return {a.x() - b.x(), a.y() - b.y(), a.z() - b.z()};
}

inline Coord& operator+=(Coord& a, Coord const& b)
{
    a.xyz[0] += b.x();
    a.xyz[1] += b.y();
    a.xyz[2] += b.z();
    return a;
}

inline Coord& operator-=(Coord& a, Coord const& b)
{
    a.xyz[0] -= b.x();
    a.xyz[1] -= b.y();
    a.xyz[2] -= b.z();
    return a;
}

inline std::ostream& operator<<(std::ostream& os, Coord const& p)
{
    return os << '(' << int(p.xyz[0]) << ' ' << int(p.xyz[1]) << ' ' << int(p.xyz[2]) << ')';
}

template<typename CoordRange>
Coord min_coords(CoordRange coords)
{
    Coord::Scalar min_x = std::numeric_limits<Coord::Scalar>::max();
    Coord::Scalar min_y = std::numeric_limits<Coord::Scalar>::max();
    Coord::Scalar min_z = std::numeric_limits<Coord::Scalar>::max();

    for (auto const& p : coords)
    {
        auto [x, y, z] = p.xyz;
        min_x = std::min(x, min_x);
        min_y = std::min(y, min_y);
        min_z = std::min(z, min_z);
    }
    return {min_x, min_y, min_z};
}

int coord_cmp(Coord const& a, Coord const& b)
{
    for (int i = 0; i < 3; ++i)
    {
        if (a.xyz[i] < b.xyz[i]) return -1;
        else if (a.xyz[i] > b.xyz[i]) return 1;
    }
    return 0;
}

bool operator==(Coord const& a, Coord const& b)
{
    return a.x() == b.x() && a.y() == b.y() && a.z() == b.z();
}

template<typename T>
concept IsCoord = std::is_same_v<T, Coord>;

namespace std {
    template<>
    struct hash<Coord>
    {
        size_t operator()(Coord const &c) const
        {
            return (c.x() << 16) ^ (c.y() << 8) ^ c.z();
        }
    };
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif // POLYCUBES_COORD_H_
