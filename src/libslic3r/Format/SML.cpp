#include "../libslic3r.h"
#include "../Model.hpp"
#include "../TriangleMesh.hpp"

#include "SML.hpp"

#include <string>

#include <boost/log/trivial.hpp>

#ifdef _WIN32
#define DIR_SEPARATOR '\\'
#else
#define DIR_SEPARATOR '/'
#endif

namespace Slic3r {

bool load_sml(const char *path, TriangleMesh *meshptr)
{
    if (meshptr == nullptr)
        return false;


    FILE *pFile = boost::nowide::fopen(path, "rb");
    if (pFile == 0)
        return false;

    char header[4];
    if (::fread(header, 4, 1, pFile) != 1) {
        BOOST_LOG_TRIVIAL(error) << "load_sml: failed to parse " << path << ". Could not read the header.";
        return false;
    }
    if (memcmp(header, "SML1", 4)) {
        BOOST_LOG_TRIVIAL(error) << "load_sml: failed to parse " << path << ". The header was invalid.";
        return false;
    }

    uint32_t crc;
    if (::fread(&crc, 4, 1, pFile) != 1) {
        BOOST_LOG_TRIVIAL(error) << "load_sml: failed to parse " << path << ". Could not read the header.";
        return false;
    }
    // FIXME Should actually check if the file matches the CRC.

    indexed_triangle_set its;

    uint8_t type;
    uint32_t length;
    while (::fread(&type, 1, 1, pFile) == 1) {
        if (::fread(&length, 4, 1, pFile) != 1) {
            BOOST_LOG_TRIVIAL(error) << "load_sml: failed to parse " << path << ". Truncated segment.";
            return false;
        }

        switch (type) {
            default: // Unsupported segment type
            case 0: // Comment
                ::fseek(pFile, length, SEEK_CUR);
                break;

            case 1: { // Float vertex list
                uint32_t num_vertices = length / 12; // 3x4 bytes per vertex
                its.vertices.reserve(its.vertices.size() + num_vertices);

                float coords[3];

                for (uint32_t i = 0; i < num_vertices; ++ i) {
                    if (::fread(coords, 4, 3, pFile) != 3) {
                        BOOST_LOG_TRIVIAL(error) << "load_sml: failed to parse " << path << ". Truncated segment.";
                        return false;
                    }
                    its.vertices.emplace_back(coords[0], coords[1], coords[2]);
                }
            } break;

            case 2: { // Double vertex list
                uint32_t num_vertices = length / 24; // 3x8 bytes per vertex
                its.vertices.reserve(its.vertices.size() + num_vertices);

                double coords[3];

                for (uint32_t i = 0; i < num_vertices; ++ i) {
                    if (::fread(coords, 8, 3, pFile) != 3) {
                        BOOST_LOG_TRIVIAL(error) << "load_sml: failed to parse " << path << ". Truncated segment.";
                        return false;
                    }
                    its.vertices.emplace_back(coords[0], coords[1], coords[2]);
                }
            } break;

            case 3: { // Triangle list
                uint32_t num_faces = length / 12; // 3x4 bytes per triangle
                its.indices.reserve(its.indices.size() + num_faces);

                uint32_t indices[3];

                for (uint32_t i = 0; i < num_faces; ++ i) {
                    if (::fread(indices, 4, 3, pFile) != 3) {
                        BOOST_LOG_TRIVIAL(error) << "load_sml: failed to parse " << path << ". Truncated segment.";
                        return false;
                    }
                    its.indices.emplace_back(indices[0], indices[1], indices[2]);
                }
            } break;
            case 4: { // Quad list
                uint32_t num_faces = length / 16; // 4x4 bytes per triangle
                its.indices.reserve(its.indices.size() + num_faces);

                uint32_t indices[4];

                for (uint32_t i = 0; i < num_faces; ++ i) {
                    if (::fread(indices, 4, 4, pFile) != 4) {
                        BOOST_LOG_TRIVIAL(error) << "load_sml: failed to parse " << path << ". Truncated segment.";
                        return false;
                    }
                    its.indices.emplace_back(indices[0], indices[1], indices[2]);
                    its.indices.emplace_back(indices[0], indices[2], indices[3]);
                }
            } break;

            case 5: { // Triangle strip
                uint32_t num_points = length / 4;
                // In testing it turned out to be vastly faster to NOT reserve space, and let libstdc++'s algorithms do it.
                //uint32_t num_faces = (num_points - 2) * 3;
                //its.indices.reserve(its.indices.size() + num_faces);

                uint32_t indices[3];
                if (::fread(indices, 4, 3, pFile) != 3) {
                    BOOST_LOG_TRIVIAL(error) << "load_sml: failed to parse " << path << ". Truncated segment.";
                    return false;
                }
                its.indices.emplace_back(indices[0], indices[1], indices[2]);

                for (uint32_t i = 3; i < num_points; ++ i) {
                    indices[i & 1] = indices[2];

                    if (::fread(&indices[2], 4, 1, pFile) != 1) {
                        BOOST_LOG_TRIVIAL(error) << "load_sml: failed to parse " << path << ". Truncated segment.";
                        return false;
                    }

                    its.indices.emplace_back(indices[0], indices[1], indices[2]);
                }
            } break;
        }
    }

    ::fclose(pFile);

    *meshptr = TriangleMesh(std::move(its));
    if (meshptr->empty()) {
        BOOST_LOG_TRIVIAL(error) << "load_sml: This SML file couldn't be read because it's empty. " << path;
        return false;
    }
    if (meshptr->volume() < 0)
        meshptr->flip_triangles();
    return true;
}

bool load_sml(const char *path, Model *model, const char *object_name_in)
{
    TriangleMesh mesh;

    bool ret = load_sml(path, &mesh);

    if (ret) {
        std::string  object_name;
        if (object_name_in == nullptr) {
            const char *last_slash = strrchr(path, DIR_SEPARATOR);
            object_name.assign((last_slash == nullptr) ? path : last_slash + 1);
        } else
           object_name.assign(object_name_in);

        model->add_object(object_name.c_str(), path, std::move(mesh));
    }

    return ret;
}

bool store_sml(const char *path, TriangleMesh *mesh)
{
    // FIXME Implement this.
    return true;
}

bool store_sml(const char *path, ModelObject *model_object)
{
    // FIXME Implement this.
    return true;
}

bool store_sml(const char *path, Model *model)
{
    // FIXME Implement this.
    return true;
}

}; // namespace Slic3r
