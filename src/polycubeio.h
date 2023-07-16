#ifndef POLYCUBES_POLYCUBEIO_H_
#define POLYCUBES_POLYCUBEIO_H_

#include "polycube.h"

#include <filesystem>
#include <fstream>
#include <memory>

class PolyCubeListFileReader
{
    static size_t constexpr PAGE_SIZE = 1000'000'000;

    template<size_t SIZE>
    struct Page
    {
        size_t index{};
        std::vector<PolyCube<SIZE>> shapes;
        PolyCube<SIZE> const& operator[](size_t i) const { return shapes[i]; }
    };

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
        m_stream->seekg(0, std::ios::end);
        m_end_pos = m_stream->tellg();

        m_polycube_count = (m_end_pos - m_begin_pos) / (m_cube_count * sizeof(Coord));

        auto page_count = (m_polycube_count - 1) / PAGE_SIZE + 1;
        m_pages.resize(page_count);
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

        Iter(PolyCubeListFileReader* parent, size_t pos)
            : m_parent{parent}, m_pos{pos}
        {
            update();
        }

        Iter(const Iter&) = default;
        Iter(Iter&&) = default;
        Iter& operator=(const Iter&) = default;
        Iter& operator=(Iter&&) = default;

        reference operator*() const { return (*m_page)[m_pos_in_page]; }
        pointer operator->() const { return &(*m_page)[m_pos_in_page]; }

        reference operator[](difference_type n) const { return *((*this) + n);}

        Iter& operator++()
        {
            ++m_pos;
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
            --m_pos;
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
            return m_pos - other.m_pos;
        }

        Iter& operator+=(difference_type n)
        {
            m_pos += n;
            update();
            return *this;
        }

        Iter& operator-=(difference_type n)
        {
            m_pos -= n;
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
            return m_parent == other.m_parent && m_pos == other.m_pos;
        }

        bool operator!=(Iter const& other) const { return !(*this == other); }

        bool operator<(Iter const& other) const { return m_pos < other.m_pos; }

        bool operator<=(Iter const& other) const { return m_pos <= other.m_pos; }

        bool operator>=(Iter const& other) const { return other < *this; }

        bool operator>(Iter const& other) const { return other <= *this; }

    private:
        void update()
        {
            auto page_idx = m_pos / PAGE_SIZE;
            if (m_page == nullptr || m_page->index != page_idx) {
                m_page = m_parent->page<SIZE>(page_idx);
            }
            m_pos_in_page = m_pos - page_idx * PAGE_SIZE;
        }

        PolyCubeListFileReader* m_parent;
        size_t m_pos{};
        std::shared_ptr<const Page<SIZE>> m_page;
        size_t m_pos_in_page{};
    };

    template <size_t SIZE> Iter<SIZE> begin() { return Iter<SIZE>{this, 0}; }

    template <size_t SIZE> Iter<SIZE> end() { return Iter<SIZE>{this, m_polycube_count}; }

    template <size_t SIZE>
    std::vector<PolyCube<SIZE>> slurp()
    {
        return std::vector<PolyCube<SIZE>>(begin<SIZE>(), end<SIZE>());
    }

private:
    template<size_t SIZE>
    std::shared_ptr<const Page<SIZE>> page(int page_idx)
    {
        auto void_result = m_pages[page_idx].lock();
        if (void_result == nullptr) {
            // load the page
            auto constexpr page_bytes = PAGE_SIZE * sizeof(PolyCube<SIZE>);
            auto const page_start = m_begin_pos + page_idx * (long)page_bytes;
            auto const actual_page_len = std::min(m_end_pos - page_start, (long)page_bytes);
            auto const actual_shape_count = actual_page_len / sizeof(PolyCube<SIZE>);
            m_stream->seekg(page_start);
            auto result = std::make_shared<Page<SIZE>>();
            result->index = page_idx;
            result->shapes.resize(actual_shape_count);
            m_stream->read(reinterpret_cast<char*>(result->shapes.data()), actual_page_len);
            m_pages[page_idx] = result;
            return result;
        } else {
            return std::reinterpret_pointer_cast<const Page<SIZE>>(void_result);
        }
    }

    std::unique_ptr<std::istream> m_stream{};
    int m_cube_count{};
    std::istream::pos_type m_begin_pos{};
    std::istream::pos_type m_end_pos{};
    size_t m_polycube_count{};

    std::vector<std::weak_ptr<void>> m_pages;
};

template<size_t SIZE>
PolyCubeListFileReader::Iter<SIZE> operator+(std::iter_difference_t<PolyCubeListFileReader::Iter<SIZE>> n, PolyCubeListFileReader::Iter<SIZE> const& iter)
{
    return iter + n;
}

template <int SIZE>
class PolyCubeListFileWriter
{
    static size_t constexpr WRITE_BUF_SIZE = 10'000'000;
public:
    explicit PolyCubeListFileWriter(std::filesystem::path const& path)
        : m_stream{std::make_unique<std::ofstream>(path, std::ios::binary | std::ios::trunc | std::ios::out)}
    {
        static_assert(std::endian::native == std::endian::little);
        const int32_t size = SIZE;
        m_stream->write("PLYCUBE1", 8);
        m_stream->write(reinterpret_cast<char const*>(&size), sizeof(int32_t));
    }

    void write(PolyCube<SIZE> const& s)
    {
        m_wbuf.push_back(s);
        if (m_wbuf.size() >= WRITE_BUF_SIZE) flush();
    }

    ~PolyCubeListFileWriter()
    {
        flush();
    }
private:
    void flush()
    {
        m_stream->write(reinterpret_cast<char const*>(m_wbuf.data()),
                        m_wbuf.size() * sizeof(PolyCube<SIZE>));
        m_wbuf.clear();
    }

    std::unique_ptr<std::ostream> m_stream{};
    std::vector<PolyCube<SIZE>> m_wbuf;
};

#endif // POLYCUBES_POLYCUBEIO_H_
