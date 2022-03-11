#include "libslic3r/Model.hpp"

#include <algorithm>

#include "slic3r/GUI/GLCanvas3D.hpp"
#include "slic3r/GUI/Gizmos/GLGizmoPainterBase.hpp"
#include "libslic3r/TriangleSelector.hpp"

#include <boost/nowide/cstdio.hpp>
#include <boost/log/trivial.hpp>

#define DEBUG_FILES

#ifdef DEBUG_FILES
#include <boost/nowide/cstdio.hpp>
#include "libslic3r/Utils.hpp"
#endif

namespace Slic3r::GUI {

//TODO close in seprate namespace

struct Triangle {
    stl_triangle_vertex_indices indices;
    Vec3f normal;
    float downward_dot_value;
    size_t index_in_its;
    Vec3i neighbours;
    float lowest_z_coord;
    // higher value of dot product of the downward direction and the two bottom edges
    float edge_dot_value;
    float weight;

    size_t order_by_z;

    //members updated during algorithm
    float strength { 0.0 };
    bool supports = false;
    bool visited = false;
    bool gathering_supports = false;
    size_t group_id = 0;
};

inline Vec3f value_to_rgbf(float minimum, float maximum, float value) {
    float ratio = 2.0f * (value - minimum) / (maximum - minimum);
    float b = std::max(0.0f, (1.0f - ratio));
    float r = std::max(0.0f, (ratio - 1.0f));
    float g = 1.0f - b - r;
    return Vec3f { r, g, b };
}

inline float face_area(const stl_vertex vertex[3]) {
    return (vertex[1] - vertex[0]).cross(vertex[2] - vertex[1]).norm() / 2;
}

inline float its_face_area(const indexed_triangle_set &its, const stl_triangle_vertex_indices face) {
    const stl_vertex vertices[3] { its.vertices[face[0]], its.vertices[face[1]], its.vertices[face[2]] };
    return face_area(vertices);
}
inline float its_face_area(const indexed_triangle_set &its, const int face_idx) {
    return its_face_area(its, its.indices[face_idx]);
}

struct SupportPlacerMesh {
    const float gather_phase_target;
    const float limit_angle_cos;

    indexed_triangle_set mesh;
    std::vector<Triangle> triangles;
    std::vector<size_t> triangle_indexes_by_z;

    explicit SupportPlacerMesh(indexed_triangle_set &&t_mesh, float dot_limit, float patch_size) :
            mesh(t_mesh), limit_angle_cos(dot_limit), gather_phase_target(patch_size) {
        auto neighbours = its_face_neighbors_par(mesh);

        for (size_t face_index = 0; face_index < mesh.indices.size(); ++face_index) {
            Vec3f normal = its_face_normal(mesh, face_index);
            //extract vertices
            std::vector<Vec3f> vertices { mesh.vertices[mesh.indices[face_index][0]],
                    mesh.vertices[mesh.indices[face_index][1]], mesh.vertices[mesh.indices[face_index][2]] };

            //sort vertices by z
            std::sort(vertices.begin(), vertices.end(), [](const Vec3f &a, const Vec3f &b) {
                return a.z() < b.z();
            });

            Vec3f lowest_edge_a = (vertices[1] - vertices[0]).normalized();
            Vec3f lowest_edge_b = (vertices[2] - vertices[0]).normalized();
            Vec3f down = -Vec3f::UnitZ();

            //TODO verify
            float weight = std::max(0.0f, (normal.dot(down) - limit_angle_cos) / (1.0f - limit_angle_cos))
                    * its_face_area(mesh, face_index);

            Triangle t { };
            t.indices = mesh.indices[face_index];
            t.normal = normal;
            t.downward_dot_value = normal.dot(down);
            t.index_in_its = face_index;
            t.neighbours = neighbours[face_index];
            t.lowest_z_coord = vertices[0].z();
            t.edge_dot_value = std::max(lowest_edge_a.dot(down), lowest_edge_b.dot(down));
            t.weight = weight;

            triangles.push_back(t);
            triangle_indexes_by_z.push_back(face_index);
        }

        std::sort(triangle_indexes_by_z.begin(), triangle_indexes_by_z.end(),
                [&](const size_t &left, const size_t &right) {
                    if (triangles[left].lowest_z_coord != triangles[right].lowest_z_coord) {
                        return triangles[left].lowest_z_coord < triangles[right].lowest_z_coord;
                    } else {
                        return triangles[left].edge_dot_value > triangles[right].edge_dot_value;
                    }
                });

        for (size_t order_index = 0; order_index < triangle_indexes_by_z.size(); ++order_index) {
            triangles[triangle_indexes_by_z[order_index]].order_by_z = order_index;
        }

        for (Triangle &triangle : triangles) {
            std::sort(begin(triangle.neighbours), end(triangle.neighbours), [&](const size_t left, const size_t right) {
                return triangles[left].order_by_z < triangles[right].order_by_z;
            });
        }
    }

    void find_support_areas() {
        size_t next_group_id = 1;
        for (const size_t &current_index : triangle_indexes_by_z) {
            Triangle &current = triangles[current_index];

            float gathering_neighbours_sum = 0;
            bool neighbour_gathering_support = 0;
            float strongest_neighbour_strength = 0;
            size_t group_id = 0;

            for (const auto &neighbour_index : current.neighbours) {
                const Triangle &neighbour = triangles[neighbour_index];
                if (!neighbour.visited) {
                    //not visited yet, ignore
                    continue;
                }

                if (neighbour.gathering_supports) {
                    gathering_neighbours_sum += neighbour.strength;
                    neighbour_gathering_support = true;
                }

                if (neighbour.strength > strongest_neighbour_strength) {
                    strongest_neighbour_strength = neighbour.strength;
                    group_id = neighbour.group_id;
                }
            }

            if (current.downward_dot_value < 0.1) {
                current.visited = true;
                current.supports = false;
                current.strength = gather_phase_target;
                current.group_id = group_id;
                current.gathering_supports = false;
                continue;
            }

            if (neighbour_gathering_support || current.weight > strongest_neighbour_strength
                    || strongest_neighbour_strength <= 0) {
                current.visited = true;
                current.supports = true;
                current.strength = std::min(gathering_neighbours_sum
                        + its_face_area(mesh, current.index_in_its), gather_phase_target);
                current.gathering_supports = current.strength < gather_phase_target;
                current.group_id = next_group_id;
                next_group_id++;
                continue;
            }

            current.visited = true;
            current.supports = false;
            current.strength = strongest_neighbour_strength - current.weight;
            current.gathering_supports = false;
            current.group_id = group_id;
        }
    }

#ifdef DEBUG_FILES
    void debug_export() const {
        Slic3r::CNumericLocalesSetter locales_setter;

        {
            std::string file_name = debug_out_path("strength.obj");

            FILE *fp = boost::nowide::fopen(file_name.c_str(), "w");
            if (fp == nullptr) {
                BOOST_LOG_TRIVIAL(error)
                << "stl_write_obj: Couldn't open " << file_name << " for writing";
                return;
            }

            for (size_t i = 0; i < triangles.size(); ++i) {
                Vec3f color = value_to_rgbf(0.0f, 2.0f * gather_phase_target, triangles[i].strength);
                for (size_t index = 0; index < 3; ++index) {
                    fprintf(fp, "v %f %f %f  %f %f %f\n", mesh.vertices[triangles[i].indices[index]](0),
                            mesh.vertices[triangles[i].indices[index]](1),
                            mesh.vertices[triangles[i].indices[index]](2), color(0),
                            color(1), color(2));
                }
                fprintf(fp, "f %zu %zu %zu\n", i * 3 + 1, i * 3 + 2, i * 3 + 3);
            }
            fclose(fp);
        }
        {
            std::string file_name = debug_out_path("groups.obj");
            FILE *fp = boost::nowide::fopen(file_name.c_str(), "w");
            if (fp == nullptr) {
                BOOST_LOG_TRIVIAL(error)
                << "stl_write_obj: Couldn't open " << file_name << " for writing";
                return;
            }

            for (size_t i = 0; i < triangles.size(); ++i) {
                Vec3f color = value_to_rgbf(0.0f, 19.0f, float(triangles[i].group_id % 20));
                for (size_t index = 0; index < 3; ++index) {
                    fprintf(fp, "v %f %f %f  %f %f %f\n", mesh.vertices[triangles[i].indices[index]](0),
                            mesh.vertices[triangles[i].indices[index]](1),
                            mesh.vertices[triangles[i].indices[index]](2), color(0),
                            color(1), color(2));
                }
                fprintf(fp, "f %zu %zu %zu\n", i * 3 + 1, i * 3 + 2, i * 3 + 3);
            }
            fclose(fp);
        }
        {
            std::string file_name = debug_out_path("sorted.obj");
            FILE *fp = boost::nowide::fopen(file_name.c_str(), "w");
            if (fp == nullptr) {
                BOOST_LOG_TRIVIAL(error)
                << "stl_write_obj: Couldn't open " << file_name << " for writing";
                return;
            }

            for (size_t i = 0; i < triangle_indexes_by_z.size(); ++i) {
                const Triangle &triangle = triangles[triangle_indexes_by_z[i]];
                Vec3f color = Vec3f { float(i) / float(triangle_indexes_by_z.size()), float(i)
                        / float(triangle_indexes_by_z.size()), 0.5 };
                for (size_t index = 0; index < 3; ++index) {
                    fprintf(fp, "v %f %f %f  %f %f %f\n", mesh.vertices[triangle.indices[index]](0),
                            mesh.vertices[triangle.indices[index]](1),
                            mesh.vertices[triangle.indices[index]](2), color(0),
                            color(1), color(2));
                }
                fprintf(fp, "f %zu %zu %zu\n", i * 3 + 1, i * 3 + 2, i * 3 + 3);
            }
            fclose(fp);
        }

        {
            std::string file_name = debug_out_path("weight.obj");
            FILE *fp = boost::nowide::fopen(file_name.c_str(), "w");
            if (fp == nullptr) {
                BOOST_LOG_TRIVIAL(error)
                << "stl_write_obj: Couldn't open " << file_name << " for writing";
                return;
            }

            for (size_t i = 0; i < triangle_indexes_by_z.size(); ++i) {
                const Triangle &triangle = triangles[triangle_indexes_by_z[i]];
                Vec3f color = value_to_rgbf(0, 10, triangle.weight);
                for (size_t index = 0; index < 3; ++index) {
                    fprintf(fp, "v %f %f %f  %f %f %f\n", mesh.vertices[triangle.indices[index]](0),
                            mesh.vertices[triangle.indices[index]](1),
                            mesh.vertices[triangle.indices[index]](2), color(0),
                            color(1), color(2));
                }
                fprintf(fp, "f %zu %zu %zu\n", i * 3 + 1, i * 3 + 2, i * 3 + 3);
            }
            fclose(fp);
        }

        {
            std::string file_name = debug_out_path("dot_value.obj");
            FILE *fp = boost::nowide::fopen(file_name.c_str(), "w");
            if (fp == nullptr) {
                BOOST_LOG_TRIVIAL(error)
                << "stl_write_obj: Couldn't open " << file_name << " for writing";
                return;
            }

            for (size_t i = 0; i < triangle_indexes_by_z.size(); ++i) {
                const Triangle &triangle = triangles[triangle_indexes_by_z[i]];
                Vec3f color = value_to_rgbf(-1, 1, triangle.downward_dot_value);
                for (size_t index = 0; index < 3; ++index) {
                    fprintf(fp, "v %f %f %f  %f %f %f\n", mesh.vertices[triangle.indices[index]](0),
                            mesh.vertices[triangle.indices[index]](1),
                            mesh.vertices[triangle.indices[index]](2), color(0),
                            color(1), color(2));
                }
                fprintf(fp, "f %zu %zu %zu\n", i * 3 + 1, i * 3 + 2, i * 3 + 3);
            }
            fclose(fp);
        }
    }
#endif

}
;

inline void do_experimental_support_placement(indexed_triangle_set mesh, TriangleSelectorGUI *selector,
        float dot_limit) {
    SupportPlacerMesh support_placer { std::move(mesh), dot_limit, 2.0 };

    support_placer.find_support_areas();

    for (const Triangle &t : support_placer.triangles) {
        if (t.supports) {
            selector->set_facet(t.index_in_its, EnforcerBlockerType::ENFORCER);
            selector->request_update_render_data();
        }
    }

    support_placer.debug_export();
}

}
