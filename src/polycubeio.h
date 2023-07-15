#ifndef POLYCUBES_POLYCUBEIO_H_
#define POLYCUBES_POLYCUBEIO_H_

#include "polycube.h"

#include <filesystem>
#include <fstream>
#include <memory>

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
        using reference = value_type const&;
        using pointer = value_type const*;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::random_access_iterator_tag;


        Iter() = default;

        Iter(std::istream* stream, std::istream::pos_type pos)
            : m_stream{stream}, m_pos{pos}
        {
            update();
        }

        Iter(const Iter&) = default;
        Iter(Iter&&) = default;
        Iter& operator=(const Iter&) = default;
        Iter& operator=(Iter&&) = default;

        reference operator*() const { return m_val; }
        pointer operator->() const { return &m_val; }

        reference operator[](difference_type n) const { return *((*this) + n);}

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

        Iter& operator--()
        {
            m_pos -= sizeof(value_type);
            update();
            return *this;
        }

        Iter operator--(int)
        {
            auto copy = *this;
            --(*this);
            return copy;
        }

        difference_type operator-(Iter const& other) const
        {
            return (m_pos - other.m_pos) / sizeof(value_type);
        }

        Iter& operator+=(difference_type n)
        {
            m_pos += n * sizeof(value_type);
            update();
            return *this;
        }

        Iter& operator-=(difference_type n)
        {
            m_pos -= n * sizeof(value_type);
            update();
            return *this;
        }

        Iter operator+(difference_type n) const
        {
            auto copy = *this;
            copy += n;
            return copy;
        }

        Iter operator-(difference_type n) const
        {
            auto copy = *this;
            copy -= n;
            return copy;
        }

        bool operator==(Iter const& other) const
        {
            return m_stream == other.m_stream && m_pos == other.m_pos;
        }

        bool operator!=(Iter const& other) const { return !(*this == other); }

        bool operator<(Iter const& other) const { return m_pos < other.m_pos; }

        bool operator<=(Iter const& other) const { return m_pos <= other.m_pos; }

        bool operator>=(Iter const& other) const { return other < *this; }

        bool operator>(Iter const& other) const { return other <= *this; }

    private:
        void update()
        {
            m_stream->clear();
            m_stream->seekg(m_pos);
            m_stream->read(reinterpret_cast<char*>(&m_val), sizeof(PolyCube<SIZE>));
            if (m_stream->good())
                m_next_pos = m_stream->tellg();
        }

        std::istream* m_stream{nullptr};
        std::istream::pos_type m_pos{};
        std::istream::pos_type m_next_pos{};
        PolyCube<SIZE> m_val;
    };

    template <size_t SIZE> Iter<SIZE> begin() { return Iter<SIZE>{m_stream.get(), m_begin_pos}; }

    template <size_t SIZE>
    Iter<SIZE> end()
    {
        m_stream->seekg(0, std::ios::end);
        auto endpos = m_stream->tellg();
        return Iter<SIZE>{m_stream.get(), endpos};
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

template<size_t SIZE>
PolyCubeListFileReader::Iter<SIZE> operator+(std::iter_difference_t<PolyCubeListFileReader::Iter<SIZE>> n, PolyCubeListFileReader::Iter<SIZE> const& iter)
{
    return iter + n;
}

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
