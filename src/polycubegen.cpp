#include "polycube.h"
#include "polycubeio.h"
#include "polycubesearch.h"
#include "util.h"

#include <cstdlib>
#include <cerrno>
#include <filesystem>
#include <format>
#include <iostream>

template <size_t SIZE>
struct escalate_impl
{
    void operator()(PolyCubeListFileReader& reader, std::filesystem::path& outfile)
    {
        auto count = gen_polycube_list(reader.begin<SIZE>(), reader.end<SIZE>(), outfile);
        std::cout << std::format("Wrote {} ({})-cubes to {}\n", count, SIZE + 1, outfile.string());
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

    {
        PolyCubeListFileWriter<1> writer{out_dir/"polycubes_1.bin"};
        writer.write({Coord{0, 0, 0}});
    }

    for (size_t count{2}; count <= maxcount; ++count) {
        auto infile = out_dir / std::format("polycubes_{}.bin", count-1);
        auto outfile = out_dir / std::format("polycubes_{}.bin", count);
        escalate(infile, outfile);
    }

    return 0;
}
