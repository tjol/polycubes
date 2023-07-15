#ifndef POLYCUBES_POLYCUBESEARCH_H_
#define POLYCUBES_POLYCUBESEARCH_H_

#include "polycube.h"
#include "polycubeio.h"

#include <unordered_set>
#include <span>
#include <thread>
#include <algorithm>
#include <execution>
#include <iterator>


long constexpr serial_chunk_size(size_t SIZE) { return 3200 / SIZE; }
inline long parallel_chunk_count() { return std::thread::hardware_concurrency() * 2; }
// 2522522 is the number of 11-cubes -> start using a cache at 13-from-12
long constexpr input_size_without_cache([[maybe_unused]] size_t SIZE) { return 2522522; }


template <size_t SIZE>
void try_adding_block(PolyCube<SIZE-1> const& orig_shape, Coord const& coord, std::unordered_set<PolyCube<SIZE>>& output)
{
    for (auto const& block : orig_shape.cubes)
    {
        if (block == coord) return;
    }

    PolyCube<SIZE> new_shape;
    std::copy(orig_shape.cubes.begin(), orig_shape.cubes.end(), new_shape.cubes.begin());
    new_shape.cubes[SIZE-1] = coord;

    auto norm_shape = new_shape.normal();
    output.insert(norm_shape);
}


template <size_t SIZE>
void find_larger(PolyCube<SIZE-1> const& orig_shape, std::unordered_set<PolyCube<SIZE>>& output)
{
    for (auto const& block : orig_shape.cubes)
    {
        try_adding_block(orig_shape, block + Coord{1, 0, 0}, output);
        try_adding_block(orig_shape, block + Coord{-1, 0, 0}, output);
        try_adding_block(orig_shape, block + Coord{0, 1, 0}, output);
        try_adding_block(orig_shape, block + Coord{0, -1, 0}, output);
        try_adding_block(orig_shape, block + Coord{0, 0, 1}, output);
        try_adding_block(orig_shape, block + Coord{0, 0, -1}, output);
    }
}

template <typename T>
void merge(std::unordered_set<T>& output, std::span<std::unordered_set<T>> additions)
{
    for (auto& addition : additions) {
        output.merge(addition);
    }
}

template<typename Iter>
concept RandomAccessPolyCubeIterator = std::random_access_iterator<Iter>
    && requires (Iter iter) {
        { *iter } -> PolyCuboidDeref;
    };

template<RandomAccessPolyCubeIterator Iter>
size_t constexpr cube_count_of_iter = std::iterator_traits<Iter>::value_type::cube_count;

template <RandomAccessPolyCubeIterator Iter>
void find_all_impl(Iter begin, Iter end, std::unordered_set<PolyCube<cube_count_of_iter<Iter>+1>>& result)
{
    size_t constexpr SIZE = cube_count_of_iter<Iter> + 1;
    static auto constexpr SERIAL_CHUNK_SIZE = serial_chunk_size(SIZE);
    static auto PARALLEL_CHUNKS = parallel_chunk_count();
    static auto PARALLEL_COUNT = PARALLEL_CHUNKS * SERIAL_CHUNK_SIZE;

    auto count = end - begin;

    if (count <= SERIAL_CHUNK_SIZE) {
        // Do these all at once, one after the other
        for (auto iter = begin; iter != end; ++iter) {
            find_larger(*iter, result);
        }
    } else if (count <= PARALLEL_COUNT) {
        // Can do all of these in N parallel chunks
        // Have to copy because PolyCubeFileListReader iterators are not thread safe
        std::vector<std::vector<PolyCube<SIZE - 1>>> chunks;
        for (long i{}; i < count; i += SERIAL_CHUNK_SIZE) {
            long chunk_len = std::min(count - i, SERIAL_CHUNK_SIZE);
            auto chunk_begin = begin + i;
            auto chunk_end = chunk_begin + chunk_len;
            chunks.push_back(std::vector<PolyCube<SIZE - 1>>(chunk_begin, chunk_end));
        }

        std::vector<std::unordered_set<PolyCube<SIZE>>> sub_results(chunks.size());

        std::vector<long> indices(chunks.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::for_each(std::execution::par, indices.begin(), indices.end(),
            [&](long i) {
                find_all_impl(chunks[i].begin(), chunks[i].end(), sub_results[i]);
            });

        merge(result, std::span{sub_results});
    } else {
        // Do super-chunks in series
        for (long i{}; i < count; i += PARALLEL_COUNT) {
            long superchunk_len = std::min(count - i, PARALLEL_COUNT);
            auto superchunk_begin = begin + i;
            auto superchunk_end = superchunk_begin + superchunk_len;

            find_all_impl(superchunk_begin, superchunk_end, result);
        }
    }
}

template <RandomAccessPolyCubeIterator Iter>
std::unordered_set<PolyCube<cube_count_of_iter<Iter> + 1>> find_all_one_larger(Iter begin, Iter end)
{
    size_t constexpr SIZE = cube_count_of_iter<Iter> + 1;
    std::unordered_set<PolyCube<SIZE>> result;
    find_all_impl(begin, end, result);
    return result;
}

template <size_t SMALL_SIZE>
std::unordered_set<PolyCube<SMALL_SIZE + 1>> find_all_one_larger(PolyCubeListFileReader& one_smaller)
{
    return find_all_one_larger(one_smaller.begin<SMALL_SIZE>(), one_smaller.end<SMALL_SIZE>());
}


#endif // POLYCUBES_POLYCUBESEARCH_H_
