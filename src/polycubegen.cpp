#include "polycube.h"
#include "polycubeio.h"
#include "polycubesearch.h"
#include "util.h"

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <filesystem>
#include <stdlib.h>
#include <errno.h>
#include <format>

template<size_t SIZE, typename Range>
concept PolyCubeRange = std::ranges::range<Range> && requires (Range r) {
    { *std::ranges::begin(r) } -> std::convertible_to<PolyCube<SIZE>>;
};

template<typename Range>
concept AnyPolyCubeRange = std::ranges::range<Range> && requires (Range r) {
    { *std::ranges::begin(r) } -> PolyCuboidDeref;
};

template<AnyPolyCubeRange Range>
using polycube_type_of = std::remove_cvref_t<decltype(*std::ranges::begin(std::declval<Range const&>()))>;

template<AnyPolyCubeRange Range>
size_t constexpr cube_count_of = polycube_type_of<Range>::cube_count;


template <AnyPolyCubeRange Range>
void dump(Range&& shapes, std::filesystem::path const& dest)
{
    PolyCubeListFileWriter<cube_count_of<Range>> writer{dest};
    for (const auto& s : shapes)
        writer.write(s);
}

template <size_t SIZE>
struct escalate_impl
{
    void operator()(PolyCubeListFileReader& reader, std::filesystem::path& outfile)
    {
        auto input_shapes = reader.slurp<SIZE>();
        auto larger_shapes = find_all_one_larger(input_shapes);
        std::cout << std::format("Writing {} ({})-cubes to {}\n",
            larger_shapes.size(), SIZE + 1, outfile.string());
        dump(larger_shapes, outfile);
    }
};

void escalate(std::filesystem::path const& infile, std::filesystem::path& outfile)
{
    PolyCubeListFileReader reader{infile};

    metaswitch<size_t, 17, escalate_impl>{}(reader.cube_count(), reader, outfile);
}

int main(int argc, char const* const* argv)
{
    using namespace std::string_view_literals;

    size_t maxcount = 6;
    std::filesystem::path out_dir{"/tmp"};

    for (int i{1}; i < argc; ++i) {
        std::string_view arg{argv[i]};

        if ((arg == "-n"sv || arg == "--maxcount"sv) && i + 1 < argc) {
            ++i;
            long val = strtol(argv[i], nullptr, 10);
            if (errno != 0) {
                perror("argument parsing error");
                return 2;
            } else if (val <= 0) {
                std::cerr << "ERROR: maxcount must be positive!\n";
                return 2;
            } else {
                maxcount = static_cast<size_t>(val);
            }
        } else if (arg == "-h"sv || arg == "--help"sv) {
            std::cout << std::format("Usage: {} [-n MAXCOUNT] [OUTDIR]\n", argv[0]);
            return 0;
        } else {
            out_dir = arg;
        }
    }

    auto seed = std::array{PolyCube<1>{Coord{0, 0, 0}}};
    dump(seed, out_dir/"polycubes_1.bin");

    for (size_t count{2}; count <= maxcount; ++count) {
        auto infile = out_dir / std::format("polycubes_{}.bin", count-1);
        auto outfile = out_dir / std::format("polycubes_{}.bin", count);
        escalate(infile, outfile);
    }

    return 0;
}
