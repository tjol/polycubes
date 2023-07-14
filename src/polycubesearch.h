#ifndef POLYCUBES_POLYCUBESEARCH_H_
#define POLYCUBES_POLYCUBESEARCH_H_

#include "polycube.h"

#include <unordered_set>
#include <span>
#include <thread>
#include <algorithm>
#include <execution>


size_t constexpr SERIAL_CHUNK_SIZE = 500;
inline size_t parallel_chunk_count() { return std::thread::hardware_concurrency() * 2; }


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

template <size_t SIZE>
void find_all_impl(std::span<PolyCube<SIZE - 1> const> one_smaller, std::unordered_set<PolyCube<SIZE>>& result)
{
    static auto PARALLEL_CHUNKS = parallel_chunk_count();
    static auto PARALLEL_COUNT = PARALLEL_CHUNKS * SERIAL_CHUNK_SIZE;

    auto count = one_smaller.size();

    if (count <= SERIAL_CHUNK_SIZE) {
        // Do these all at once, one after the other
        for (auto const& small_shape : one_smaller) {
            find_larger(small_shape, result);
        }
    } else if (count <= PARALLEL_COUNT) {
        // Can do all of these in N parallel chunks
        std::vector<std::span<PolyCube<SIZE - 1> const>> chunks;
        for (size_t i{}; i < count; i += SERIAL_CHUNK_SIZE) {
            size_t chunk_len = std::min(count - i, SERIAL_CHUNK_SIZE);
            chunks.push_back(one_smaller.subspan(i, chunk_len));
        }

        std::vector<std::unordered_set<PolyCube<SIZE>>> sub_results(chunks.size());
        
        std::vector<size_t> indices(chunks.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::for_each(std::execution::par, indices.begin(), indices.end(),
            [&](size_t i) {
                find_all_impl(chunks[i], sub_results[i]);
            });
        
        merge(result, std::span{sub_results});
    } else {
        // Do super-chunks in series
        for (size_t i{}; i < count; i += PARALLEL_COUNT) {
            size_t superchunk_len = std::min(count - i, PARALLEL_COUNT);
            auto superchunk = one_smaller.subspan(i, superchunk_len);

            find_all_impl(superchunk, result);
        }
    }
}

template <size_t SMALL_SIZE>
std::unordered_set<PolyCube<SMALL_SIZE + 1>> find_all_one_larger(std::span<PolyCube<SMALL_SIZE> const> one_smaller)
{
    size_t constexpr SIZE = SMALL_SIZE + 1;
    std::unordered_set<PolyCube<SIZE>> result;
    find_all_impl<SIZE>(one_smaller, result);
    return result;
}

template <size_t SMALL_SIZE>
std::unordered_set<PolyCube<SMALL_SIZE + 1>> find_all_one_larger(std::span<PolyCube<SMALL_SIZE>> one_smaller)
{
    return find_all_one_larger(std::span<PolyCube<SMALL_SIZE> const>{one_smaller});
}

template <size_t SMALL_SIZE>
std::unordered_set<PolyCube<SMALL_SIZE + 1>> find_all_one_larger(std::vector<PolyCube<SMALL_SIZE>> const& one_smaller)
{
    return find_all_one_larger(std::span<PolyCube<SMALL_SIZE> const>{one_smaller});
}


#endif // POLYCUBES_POLYCUBESEARCH_H_
