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
#include <iterator>

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

        m_begin_pos = m_stream->tellg();
    }

    int cube_count() const { return m_cube_count; }

    template <size_t SIZE>
    class Iter
    {
    public:
        using value_type = PolyCube<SIZE>;
        using reference = value_type&;
        using pointer = value_type*;
        using difference_type = std::ptrdiff_t;
        using const_reference = value_type const&;
        using iterator_category = std::input_iterator_tag;


        Iter(std::istream& stream, std::istream::pos_type pos)
            : m_stream{stream}, m_pos{pos}
        {
            update();
        }

        Iter(const Iter&) = default;

        reference operator*() { return m_val; }
        const_reference operator*() const { return m_val; }

        Iter& operator++()
        {
            m_pos = m_next_pos;
            update();
            return *this;
        }

        Iter operator++(int)
        {
            auto copy = *this;
            ++(*this);
            return copy;
        }

        bool operator==(Iter const& other)
        {
            return &m_stream == &other.m_stream && m_pos == other.m_pos;
        }

    private:
        void update()
        {
            m_stream.clear();
            m_stream.seekg(m_pos);
            m_stream.read(reinterpret_cast<char*>(&m_val), sizeof(PolyCube<SIZE>));
            if (m_stream.good())
                m_next_pos = m_stream.tellg();
        }

        std::istream& m_stream;
        std::istream::pos_type m_pos{};
        std::istream::pos_type m_next_pos{};
        PolyCube<SIZE> m_val;
    };

    template <size_t SIZE> Iter<SIZE> begin() { return Iter<SIZE>{*m_stream, m_begin_pos}; }

    template <size_t SIZE>
    Iter<SIZE> end()
    {
        m_stream->seekg(0, std::ios::end);
        auto endpos = m_stream->tellg();
        return Iter<SIZE>{*m_stream, endpos};
    }

    template <size_t SIZE>
    std::vector<PolyCube<SIZE>> slurp()
    {
        return std::vector<PolyCube<SIZE>>(begin<SIZE>(), end<SIZE>());
    }

private:
    std::unique_ptr<std::istream> m_stream{};
    std::istream::pos_type m_begin_pos{};
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
