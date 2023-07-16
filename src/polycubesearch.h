#ifndef POLYCUBES_POLYCUBESEARCH_H_
#define POLYCUBES_POLYCUBESEARCH_H_

#include "polycube.h"
#include "polycubeio.h"
#include "util.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <execution>
#include <format>
#include <functional>
#include <iostream>
#include <iterator>
#include <mutex>
#include <set>
#include <thread>


long constexpr serial_chunk_size(size_t SIZE) { return 3200 / SIZE; }
inline long parallel_chunk_count() { return std::thread::hardware_concurrency(); }
// 2522522 is the number of 11-cubes -> start using a cache at 13-from-12
long constexpr input_size_without_cache([[maybe_unused]] size_t SIZE) { return 2522522; }


template <size_t SIZE>
void try_adding_block(PolyCube<SIZE-1> const& orig_shape, Coord const& coord, std::set<PolyCube<SIZE>>& output)
{
    for (auto const& block : orig_shape.cubes) {
        if (block == coord) return;
    }

    PolyCube<SIZE> new_shape;
    std::copy(orig_shape.cubes.begin(), orig_shape.cubes.end(), new_shape.cubes.begin());
    new_shape.cubes[SIZE-1] = coord;

    auto norm_shape = new_shape.normal();
    output.insert(norm_shape);
}


template <size_t SIZE>
void find_larger(PolyCube<SIZE-1> const& orig_shape, std::set<PolyCube<SIZE>>& output)
{
    for (auto const& block : orig_shape.cubes) {
        try_adding_block(orig_shape, block + Coord{1, 0, 0}, output);
        try_adding_block(orig_shape, block + Coord{-1, 0, 0}, output);
        try_adding_block(orig_shape, block + Coord{0, 1, 0}, output);
        try_adding_block(orig_shape, block + Coord{0, -1, 0}, output);
        try_adding_block(orig_shape, block + Coord{0, 0, 1}, output);
        try_adding_block(orig_shape, block + Coord{0, 0, -1}, output);
    }
}

template <typename T, typename Range>
void merge_all(std::set<T>& output, Range&& additions)
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
void find_all_impl(Iter begin, Iter end, std::set<PolyCube<cube_count_of_iter<Iter>+1>>& result)
{
    size_t constexpr SIZE = cube_count_of_iter<Iter> + 1;
    static auto constexpr SERIAL_CHUNK_SIZE = serial_chunk_size(SIZE);
    static auto PARALLEL_CHUNKS = parallel_chunk_count();
    static auto PARALLEL_COUNT = PARALLEL_CHUNKS * SERIAL_CHUNK_SIZE;

    long count = end - begin;

    if (count <= SERIAL_CHUNK_SIZE) {
        // Do these all at once, one after the other
        for (auto iter = begin; iter != end; ++iter) {
            find_larger(*iter, result);
        }
    } else if (count <= PARALLEL_COUNT) {
        // Can do all of these in N parallel chunks
        // Have to copy because PolyCubeListFileReader iterators are not thread safe
        std::vector<std::vector<PolyCube<SIZE - 1>>> chunks;
        for (long i{}; i < count; i += SERIAL_CHUNK_SIZE) {
            long chunk_len = std::min(count - i, SERIAL_CHUNK_SIZE);
            auto chunk_begin = begin + i;
            auto chunk_end = chunk_begin + chunk_len;
            chunks.push_back(std::vector<PolyCube<SIZE - 1>>(chunk_begin, chunk_end));
        }

        std::vector<std::set<PolyCube<SIZE>>> sub_results(chunks.size());

        std::vector<long> indices(chunks.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::for_each(std::execution::par, indices.begin(), indices.end(),
            [&](long i) {
                find_all_impl(chunks[i].begin(), chunks[i].end(), sub_results[i]);
            });

        merge_all(result, sub_results);
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
std::set<PolyCube<cube_count_of_iter<Iter> + 1>> find_all_one_larger(Iter begin, Iter end)
{
    size_t constexpr SIZE = cube_count_of_iter<Iter> + 1;
    std::set<PolyCube<SIZE>> result;
    find_all_impl(begin, end, result);
    return result;
}

template<size_t SIZE>
class PolyCubeListGenerator
{
public:
    PolyCubeListGenerator(std::filesystem::path outfile)
    : m_out_file{std::move(outfile)},
      m_cache_file{m_out_file.parent_path() / std::format(".{}.tmp.1", m_out_file.filename().string())},
      m_tmp_cache_file{m_out_file.parent_path() / std::format(".{}.tmp.2", m_out_file.filename().string())}
    {
    }

    template<typename Iter>
    long operator()(Iter seed_begin, Iter seed_end)
    {
        // Start the worker thread
        m_count = 0;
        m_merge_worker_thread = std::jthread{std::bind(&PolyCubeListGenerator<SIZE>::merge_worker, this)};

        // Start the clock (for logging)
        auto t0 = std::chrono::system_clock::now();

        long seed_count = seed_end - seed_begin;
        auto chunk_size = input_size_without_cache(SIZE);

        for (long i{}; i < seed_count; i += chunk_size) {
            long chunk_len = std::min(seed_count - i, chunk_size);

            auto chunk_begin = seed_begin + i;
            auto chunk_end = chunk_begin + chunk_len;
            bool is_last_chunk = chunk_end == seed_end;

            // Do the search on this chunk
            auto chunk_result = find_all_one_larger(chunk_begin, chunk_end);

            // Hand the result over
            {
                std::unique_lock lock{m_result_mutex};
                m_done = is_last_chunk;
                m_result_chunks.emplace_back(std::move(chunk_result));
            }
            m_result_condvar.notify_one();

            if (!is_last_chunk || i != 0) {
                using Duration = std::chrono::system_clock::duration;
                auto t = std::chrono::system_clock::now();
                Duration dt = t - t0;
                auto n_done = chunk_end - seed_begin;
                auto progress = double(n_done) / double(seed_count);
                auto expected_duration = Duration{(Duration::rep)(dt.count() * (1.0 / progress))};
                auto eta = t0 + expected_duration;

                std::cout << std::format("[{0}] generating ({2})-cubes: {3:.3}% ({4}/{5}); ETA (optimistic) {1}\n",
                    strftime_local("%FT%T", t), strftime_local("%R", eta),
                    SIZE, progress * 100.0, n_done, seed_count);
            }
        }
        // Wait for the result to be written
        m_merge_worker_thread.join();

        return m_count;
    }

private:
    void merge_worker()
    {
        std::vector<std::set<PolyCube<SIZE>>> new_chunks;
        bool done = false;

        while (!done) {
            {
                std::unique_lock result_lock{m_result_mutex};
                if (!m_result_chunks.empty()) {
                    new_chunks = std::move(m_result_chunks);
                }
                done = m_done;

                if (new_chunks.empty() && !done) {
                    m_result_condvar.wait(result_lock);
                }
            }

            if (new_chunks.size() > 1) {
                std::cout << "WARNING: " << new_chunks.size()
                    << " results in queue; IO is slower than compute!\n";
            }


            if (!new_chunks.empty()) merge_results(new_chunks);
        }

        // commit the result
        if (std::filesystem::exists(m_cache_file)) {
            std::filesystem::rename(m_cache_file, m_out_file);
        }
    }

    void merge_results(std::vector<std::set<PolyCube<SIZE>>>& new_chunks)
    {
        auto new_chunks_span = std::span{new_chunks};
        PolyCubeListFileWriter<SIZE> cache_out{m_tmp_cache_file};

        auto old_count = m_count;
        m_count = 0;

        auto output_func = [&](auto const& pc) {
            ++m_count;
            cache_out.write(pc);
        };

        if (old_count == 0) {
            // first chunk(s)
            std::span<PolyCube<SIZE>> nullspan;
            merge_uniq(nullspan, new_chunks_span, output_func);
        } else {
            PolyCubeListFileReader cache{m_cache_file};
            merge_uniq(cache.range<SIZE>(), new_chunks_span, output_func);
        }

        std::filesystem::remove(m_cache_file);
        std::filesystem::rename(m_tmp_cache_file, m_cache_file);
        if (!(old_count == 0 && m_done)) {
            std::cout << std::format("[{}] wrote {} ({})-cubes to disk (was {})\n",
                strftime_local("%FT%T", std::chrono::system_clock::now()),
                m_count, SIZE, old_count);
        }

        new_chunks.clear();
    }

    std::filesystem::path m_out_file;
    std::filesystem::path m_cache_file;
    std::filesystem::path m_tmp_cache_file;

    std::jthread m_merge_worker_thread;
    std::mutex m_result_mutex;
    std::condition_variable m_result_condvar;
    long m_count{};

    std::vector<std::set<PolyCube<SIZE>>> m_result_chunks;
    bool m_done{};
};

template <RandomAccessPolyCubeIterator Iter>
long gen_polycube_list(Iter seed_begin, Iter seed_end, std::filesystem::path outfile)
{
    size_t constexpr SIZE = cube_count_of_iter<Iter> + 1;

    PolyCubeListGenerator<SIZE> gen{outfile};

    return gen(seed_begin, seed_end);
}


#endif // POLYCUBES_POLYCUBESEARCH_H_
