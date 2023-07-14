#include "polycube.h"
#include "polycubeio.h"
#include "polycubesearch.h"

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
void dump(Range shapes, std::filesystem::path const& dest)
{
    PolyCubeListFileWriter<cube_count_of<Range>> writer{dest};
    for (const auto& s : shapes)
        writer.write(s);
}

template <size_t SIZE>
void escalate_impl(PolyCubeListFileReader& reader, std::filesystem::path& outfile)
{
    auto input_shapes = reader.slurp<SIZE>();
    auto larger_shapes = find_all_one_larger(input_shapes);
    dump(larger_shapes, outfile);
}

void escalate(std::filesystem::path const& infile, std::filesystem::path& outfile)
{
    PolyCubeListFileReader reader{infile};

    switch (reader.cube_count()) {
    case 1:
        escalate_impl<1>(reader, outfile);
        break;
    case 2:
        escalate_impl<2>(reader, outfile);
        break;
    case 3:
        escalate_impl<3>(reader, outfile);
        break;
    case 4:
        escalate_impl<4>(reader, outfile);
        break;
    case 5:
        escalate_impl<5>(reader, outfile);
        break;
    case 6:
        escalate_impl<6>(reader, outfile);
        break;
    case 7:
        escalate_impl<7>(reader, outfile);
        break;
    case 8:
        escalate_impl<8>(reader, outfile);
        break;
    case 9:
        escalate_impl<9>(reader, outfile);
        break;
    case 10:
        escalate_impl<10>(reader, outfile);
        break;
    case 11:
        escalate_impl<11>(reader, outfile);
        break;
    case 12:
        escalate_impl<12>(reader, outfile);
        break;
    case 13:
        escalate_impl<13>(reader, outfile);
        break;
    case 14:
        escalate_impl<14>(reader, outfile);
        break;
    case 15:
        escalate_impl<15>(reader, outfile);
        break;
    case 16:
        escalate_impl<16>(reader, outfile);
        break;
    case 17:
        escalate_impl<17>(reader, outfile);
        break;
    default:
        throw std::runtime_error("not implemented");
    }
}

int main(int argc, char const* const* argv)
{
    using namespace std::string_view_literals;

    size_t maxcount = 2;
    std::filesystem::path out_dir{"."};

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
