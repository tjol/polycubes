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
#include <format>


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

template <RandomAccessPolyCubeIterator Iter>
size_t gen_polycube_list(Iter seed_begin, Iter seed_end, std::filesystem::path outfile)
{
    size_t constexpr SIZE = cube_count_of_iter<Iter> + 1;

    auto cachefile1 = outfile.parent_path() / std::format(".{}.tmp.1", outfile.filename().string());
    auto cachefile2 = outfile.parent_path() / std::format(".{}.tmp.2", outfile.filename().string());

    auto seed_count = seed_end - seed_begin;
    auto chunk_size = input_size_without_cache(SIZE);

    // Abbreviated version for small numbers
    if (seed_count <= chunk_size) {
        PolyCubeListFileWriter<SIZE> writer{outfile};
        auto result = find_all_one_larger(seed_begin, seed_end);
        for (auto const& pc : result) writer.write(pc);
        return result.size();
    }

    size_t count = 0;

    for (long i{}; i < seed_count; i += chunk_size) {
        long chunk_len = std::min(seed_count - i, chunk_size);

        auto chunk_begin = seed_begin + i;
        auto chunk_end = chunk_begin + chunk_len;
        auto chunk_result = find_all_one_larger(chunk_begin, chunk_end);

        auto result_vec = std::vector(chunk_result.begin(), chunk_result.end());
        std::sort(std::execution::par, result_vec.begin(), result_vec.end());

        if (i == 0) {
            PolyCubeListFileWriter<SIZE> writer{cachefile1};
            for (auto const& pc : result_vec) writer.write(pc);
            std::cout << std::format("- SYNC ({}) [{} of {}] saved {}\n",
                SIZE, i, seed_count, result_vec.size());
        } else {
            {
                PolyCubeListFileReader cache{cachefile1};
                PolyCubeListFileWriter<SIZE> cache_out{cachefile2};

                auto cache_begin = cache.begin<SIZE>();
                auto cache_end = cache.end<SIZE>();
                auto old_count = cache_end - cache_begin;
                count = 0;
                merge_uniq(cache_begin, cache_end, result_vec.begin(), result_vec.end(),
                    [&](auto const& pc) {
                        ++count;
                        cache_out.write(pc);
                    });
                std::cout << std::format("- SYNC ({}) [{} of {}] saved {} (was {}, dup {})\n",
                    SIZE, i, seed_count, count, old_count, (old_count + result_vec.size()) - count);
            }
            std::filesystem::remove(cachefile1);
            std::filesystem::rename(cachefile2, cachefile1);
        }
    }

    std::filesystem::rename(cachefile1, outfile);

    return count;
}


#endif // POLYCUBES_POLYCUBESEARCH_H_
