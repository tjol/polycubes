#ifndef POLYCUBES_POLYCUBEIO_H_
#define POLYCUBES_POLYCUBEIO_H_

#include "polycube.h"

#include <filesystem>
#include <memory>
#include <fstream>
#include <concepts>
#include <array>
#include <cstdint>
#include <bit>
#include <iostream>

class PolyCubeListFileReader
{
public:
    explicit PolyCubeListFileReader(std::filesystem::path const& path)
        : m_stream{std::make_unique<std::ifstream>(path, std::ios::binary | std::ios::in)}
    {
        using namespace std::literals::string_literals;

        std::string magic(8, '\0');
        m_stream->read(magic.data(), 8);
        if (magic != "PLYCUBE1"s) throw std::runtime_error("Invalid file format");

        int32_t cube_count = -1;
        m_stream->read(reinterpret_cast<char*>(&cube_count), sizeof(int32_t));
        if (!m_stream->good()) throw std::runtime_error("Error reading file (sz)");

        m_cube_count = cube_count;
    }

    int cube_count() const { return m_cube_count; }

    template <size_t SIZE, typename Func>
    requires std::invocable<Func, PolyCube<SIZE> const&>
    void for_each(Func func)
    {
        static_assert(std::endian::native == std::endian::little);
        while (!m_stream->eof()) {
            PolyCube<SIZE> s;
            m_stream->read(reinterpret_cast<char*>(&s), sizeof(PolyCube<SIZE>));
            auto count = m_stream->gcount();
            if (count == 0 && m_stream->eof()) {
                // clean end
                return;
            } else if (count != sizeof(PolyCube<SIZE>) || m_stream->fail()) {
                // unclean end
                throw std::runtime_error("Error reading file (v)");
            }
            func(std::move(s));
        }
    }

    template <size_t SIZE>
    std::vector<PolyCube<SIZE>> slurp()
    {
        std::vector<PolyCube<SIZE>> result;
        for_each<SIZE>([&result] (auto const& PolyCube) {
            result.push_back(PolyCube);
        });
        return result;
    }

private:
    std::unique_ptr<std::istream> m_stream{};
    int m_cube_count{};
};

template <int SIZE>
class PolyCubeListFileWriter
{
public:
    explicit PolyCubeListFileWriter(std::filesystem::path const& path)
        : m_stream{std::make_unique<std::ofstream>(path, std::ios::binary | std::ios::trunc | std::ios::out)}
    {
        const int32_t size = SIZE;
        m_stream->write("PLYCUBE1", 8);
        m_stream->write(reinterpret_cast<char const*>(&size), sizeof(int32_t));
    }

    void write(PolyCube<SIZE> const& s)
    {
        static_assert(std::endian::native == std::endian::little);
        m_stream->write(reinterpret_cast<char const*>(&s), sizeof(PolyCube<SIZE>));
    }
private:
    std::unique_ptr<std::ostream> m_stream{};
};

#endif // POLYCUBES_POLYCUBEIO_H_
