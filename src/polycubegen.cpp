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

void escalate(PolyCubeListFileReader& reader, std::filesystem::path& outfile)
{
    metaswitch<size_t, 17, escalate_impl>{}(reader.cube_count(), reader, outfile);
}

int main(int argc, char const* const* argv)
{
    using namespace std::string_view_literals;

    size_t maxcount = 6;
    std::filesystem::path out_dir{"."};
    std::filesystem::path seed_file;

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
        } else if ((arg == "-s"sv || arg == "--seed"sv) && i + 1 < argc) {
            seed_file = argv[++i];
        } else if (arg == "-h"sv || arg == "--help"sv) {
            std::cout << std::format("Usage: {} [-n MAXCOUNT] [-s SEED_FILE] [OUTDIR]\n", argv[0]);
            return 0;
        } else {
            out_dir = arg;
        }
    }

    if (seed_file.empty()) {
        seed_file = out_dir/"polycubes_1.bin";
        PolyCubeListFileWriter<1> writer{seed_file};
        writer.write({Coord{0, 0, 0}});
    }

    size_t count{};

    do {
        PolyCubeListFileReader reader{seed_file};
        count = reader.cube_count() + 1;
        auto outfile = out_dir / std::format("polycubes_{}.bin", count);
        escalate(reader, outfile);
        seed_file = outfile;
    } while (count < maxcount);

    return 0;
}
