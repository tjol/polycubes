#include "polycubeio.h"
#include "util.h"

#include <cmath>
#include <fstream>
#include <array>
#include <format>

template <size_t SIZE>
struct convert_impl
{
    using vec3 = std::array<float, 3>;

    struct SquareFace {
        std::array<vec3, 4> vertices;
        vec3 normal;
    };

    static std::vector<SquareFace> make_faces(Coord cube)
    {
        std::vector<SquareFace> result;
        result.push_back(SquareFace{
            .vertices={
                vec3{cube.x() - 0.5f, cube.y() + 0.5f, cube.z() - 0.5f},
                vec3{cube.x() + 0.5f, cube.y() + 0.5f, cube.z() - 0.5f},
                vec3{cube.x() + 0.5f, cube.y() - 0.5f, cube.z() - 0.5f},
                vec3{cube.x() - 0.5f, cube.y() - 0.5f, cube.z() - 0.5f},
            },
            .normal={0.f, 0.f, -1.f}
        });
        result.push_back(SquareFace{
            .vertices={
                vec3{cube.x() - 0.5f, cube.y() - 0.5f, cube.z() + 0.5f},
                vec3{cube.x() + 0.5f, cube.y() - 0.5f, cube.z() + 0.5f},
                vec3{cube.x() + 0.5f, cube.y() + 0.5f, cube.z() + 0.5f},
                vec3{cube.x() - 0.5f, cube.y() + 0.5f, cube.z() + 0.5f},
            },
            .normal={0.f, 0.f, 1.f}
        });
        result.push_back(SquareFace{
            .vertices={
                vec3{cube.x() - 0.5f, cube.y() - 0.5f, cube.z() - 0.5f},
                vec3{cube.x() + 0.5f, cube.y() - 0.5f, cube.z() - 0.5f},
                vec3{cube.x() + 0.5f, cube.y() - 0.5f, cube.z() + 0.5f},
                vec3{cube.x() - 0.5f, cube.y() - 0.5f, cube.z() + 0.5f},
            },
            .normal={0.f, -1.f, 0.f}
        });
        result.push_back(SquareFace{
            .vertices={
                vec3{cube.x() - 0.5f, cube.y() + 0.5f, cube.z() + 0.5f},
                vec3{cube.x() + 0.5f, cube.y() + 0.5f, cube.z() + 0.5f},
                vec3{cube.x() + 0.5f, cube.y() + 0.5f, cube.z() - 0.5f},
                vec3{cube.x() - 0.5f, cube.y() + 0.5f, cube.z() - 0.5f},
            },
            .normal={0.f, 1.f, 0.f}
        });
        result.push_back(SquareFace{
            .vertices={
                vec3{cube.x() - 0.5f, cube.y() - 0.5f, cube.z() + 0.5f},
                vec3{cube.x() - 0.5f, cube.y() + 0.5f, cube.z() + 0.5f},
                vec3{cube.x() - 0.5f, cube.y() + 0.5f, cube.z() - 0.5f},
                vec3{cube.x() - 0.5f, cube.y() - 0.5f, cube.z() - 0.5f},
            },
            .normal={-1.f, 0.f, 0.f}
        });
        result.push_back(SquareFace{
            .vertices={
                vec3{cube.x() + 0.5f, cube.y() - 0.5f, cube.z() - 0.5f},
                vec3{cube.x() + 0.5f, cube.y() + 0.5f, cube.z() - 0.5f},
                vec3{cube.x() + 0.5f, cube.y() + 0.5f, cube.z() + 0.5f},
                vec3{cube.x() + 0.5f, cube.y() - 0.5f, cube.z() + 0.5f},
            },
            .normal={1.f, 0.f, 0.f}
        });

        return result;
    }

    static void translate_vertices(std::vector<SquareFace>& faces, vec3 d)
    {
        for (auto& f : faces) {
            for (auto& v : f.vertices) {
                v[0] += d[0];
                v[1] += d[1];
                v[2] += d[2];
            }
        }
    }

    static std::vector<SquareFace> make_faces(PolyCube<SIZE> const& pc)
    {
        std::vector<SquareFace> result;

        for (const auto& cube : pc.cubes) {
            auto cube_faces = make_faces(cube);
            std::copy(cube_faces.begin(), cube_faces.end(), std::back_inserter(result));
        }

        return result;
    }

    static std::vector<SquareFace> make_faces(std::vector<PolyCube<SIZE>> const& pcs, size_t grid_width, float spacing)
    {
        std::vector<SquareFace> result;

        size_t x{}, y{};

        for (const auto& pc : pcs)
        {
            auto pc_faces = make_faces(pc);

            vec3 offset{0.f, 0.f, 0.f};
            offset[0] = spacing * x;
            offset[1] = spacing * y;

            translate_vertices(pc_faces, offset);

            std::copy(pc_faces.begin(), pc_faces.end(), std::back_inserter(result));

            if (++x >= grid_width) {
                x = 0;
                ++y;
            }
        }
        
        return result;
    }


    void operator()(PolyCubeListFileReader& reader, std::filesystem::path& outfile) const
    {
        auto polycubes = reader.slurp<SIZE>();
        auto count = polycubes.size();

        // Spread them out on a grid
        auto grid_width = static_cast<size_t>(std::sqrt(count));
        auto spacing = 2 * SIZE;

        auto faces = make_faces(polycubes, grid_width, (float)spacing);

        std::ofstream ofs{outfile, std::ios::out | std::ios::trunc};

        // write vertices
        ofs << "# List of vertices\n";
        for (auto const& f : faces) {
            for (auto const& v : f.vertices) {
                ofs << std::format("v {} {} {}\n", v[0], v[1], v[2]);
            }
        }
        // write vertex normals
        ofs << "# List of vertex normals\n";
        for (auto const& f : faces) {
            ofs << std::format("vn {} {} {}\n", f.normal[0], f.normal[1], f.normal[2]);
        }
        // write faces
        ofs << "# List of faces\n";
        for (size_t i{}; i < faces.size(); ++i) {
            size_t v0 = 4 * i + 1;
            size_t vn = i + 1;
            ofs << std::format("f {}//{} {}//{} {}//{} {}//{}\n",
                v0, vn, v0+1, vn, v0+2, vn, v0+3, vn);
        }
    }
};

void convert(std::filesystem::path const& infile, std::filesystem::path& outfile)
{
    PolyCubeListFileReader reader{infile};

    metaswitch<size_t, 12, convert_impl>{}(reader.cube_count(), reader, outfile);
}

int main(int argc, char const* const* argv)
{
    using namespace std::string_literals;
    using namespace std::string_view_literals;

    std::filesystem::path infile;
    std::filesystem::path outfile;

    if (argc >= 2 && (argv[1] == "-h"sv || argv[1] == "--help"sv)) {
        std::cout << std::format("Usage: {} INFILE [OUTFILE]\n", argv[0]);
        return 0;
    } else if (argc == 3) {
        infile = argv[1];
        outfile = argv[2];
    } else if (argc == 2) {
        infile = argv[1];
        if (infile.extension() == ".bin"sv) {
            outfile = infile.parent_path() / (infile.stem().string() + ".obj"s);
        } else {
            outfile = infile.parent_path() / (infile.filename().string() + ".obj"s);
        }
    } else {
        std::cerr << "Invalid argument count\n";
        return 2;
    }

    convert(infile, outfile);
    return 0;
}
