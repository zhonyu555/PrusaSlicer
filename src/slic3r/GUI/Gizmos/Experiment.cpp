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
    size_t index_in_its;
    Vec3i neighbours;
    Vec3f z_coords_sorted_asc;
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

        auto z_coords_sorted = [&](const size_t &face_index) {
            const auto &face = mesh.indices[face_index];
            Vec3f z_coords = Vec3f { mesh.vertices[face.x()].z(), mesh.vertices[face.y()].z(), mesh.vertices[face.z()].z() };
            ;
            std::sort(z_coords.data(), z_coords.data() + 3);
            return z_coords;
        };

        for (size_t face_index = 0; face_index < mesh.indices.size(); ++face_index) {
            Vec3f normal = its_face_normal(mesh, face_index);
            triangles.push_back(
                    Triangle { mesh.indices[face_index], normal, face_index, neighbours[face_index], z_coords_sorted(face_index) });
            triangle_indexes_by_z.push_back(face_index);
        }

        std::sort(triangle_indexes_by_z.begin(), triangle_indexes_by_z.end(), [&](const size_t &left, const size_t &right) {
            for (int i = 0; i < 2; ++i) {
                if (triangles[left].z_coords_sorted_asc.data()[i] < triangles[right].z_coords_sorted_asc.data()[i]) {
                    return true;
                }
                if (triangles[left].z_coords_sorted_asc.data()[i] > triangles[right].z_coords_sorted_asc.data()[i]) {
                    return false;
                }
            }
            // if two bottom points of both triangles have same z heights
            // assume neighbours (but do not rely on that), so one edge is probably common

            // find the other edge of both triangles that originates in the lowest point

            //extract vertices
            std::vector<Vec3f> vertices_left { mesh.vertices[triangles[left].indices[0]], mesh.vertices[triangles[left].indices[1]], mesh.vertices[triangles[left].indices[2]]};

            std::vector<Vec3f> vertices_right { mesh.vertices[triangles[right].indices[0]], mesh.vertices[triangles[right].indices[1]],
                    mesh.vertices[triangles[right].indices[2]] };

            //sort vertices by z
            std::sort(vertices_left.begin(), vertices_left.end(), [](const Vec3f &a, const Vec3f &b) {
                return a.z() < b.z();
            });

            std::sort(vertices_right.begin(), vertices_right.end(), [](const Vec3f &a, const Vec3f &b) {
                return a.z() < b.z();
            });

            //get the other edge
            Vec3f lowest_edge_left = (vertices_left[2] - vertices_left[0]).normalized();
            Vec3f lowest_edge_right = (vertices_right[2] - vertices_right[0]).normalized();

            Vec3f down = -Vec3f::UnitZ();

            //Pick the one that is more downwards
            return lowest_edge_left.dot(down) > lowest_edge_right.dot(down);
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

    const float calculate_support_cost(float dot_product, size_t index_in_its) const {
        float weight = std::max(0.0f, (dot_product - limit_angle_cos) / (1.0f - limit_angle_cos));
        return weight * its_face_area(mesh, index_in_its);
    }

    void find_support_areas() {
        size_t next_group_id = 1;
        for (const size_t &current_index : triangle_indexes_by_z) {
            Triangle &current = triangles[current_index];
            float dot_product = current.normal.dot(-Vec3f::UnitZ());

            float neighbours_strength_sum = 0;
            size_t group_id = 0;
            for (const auto &neighbour_index : current.neighbours) {
                const Triangle &neighbour = triangles[neighbour_index];
                if (!neighbour.visited) {
                    //not visited yet, ignore
                    continue;
                } else {
                    if (group_id == 0) {
                        group_id = neighbour.group_id;
                    }

                    neighbours_strength_sum += neighbour.strength;
                    float support_cost = calculate_support_cost(dot_product, current.index_in_its);
                    if (neighbour.gathering_supports || neighbour.strength < support_cost) {
                        continue;
                    } else {
                        current.strength = std::max(0.0f, neighbour.strength - support_cost);
                        current.visited = true;
                        current.supports = false;
                        current.gathering_supports = false;
                        current.group_id = neighbour.group_id;
                        break;
                    }
                }
            }

            if (!current.visited) {
                if (dot_product > 0.2) {
                    current.supports = true;
                    current.visited = true;
                    current.strength = neighbours_strength_sum + its_face_area(mesh, current.index_in_its);
                    current.gathering_supports = current.strength < gather_phase_target;
                    current.group_id = next_group_id;
                    next_group_id++;
                } else {
                    current.strength = gather_phase_target;
                    current.visited = true;
                    current.supports = false;
                    current.gathering_supports = false;
                    current.group_id = group_id;
                }
            }
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

            for (int i = 0; i < triangles.size(); ++i) {
                Vec3f color = value_to_rgbf(0.0f, 2.0f * gather_phase_target, triangles[i].strength);
                for (int index = 0; index < 3; ++index) {
                    fprintf(fp, "v %f %f %f  %f %f %f\n", mesh.vertices[triangles[i].indices[index]](0),
                            mesh.vertices[triangles[i].indices[index]](1), mesh.vertices[triangles[i].indices[index]](2), color(0),
                            color(1), color(2));
                }
                fprintf(fp, "f %d %d %d\n", i * 3 + 1, i * 3 + 2, i * 3 + 3);
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
                Vec3f color = value_to_rgbf(0.0f, 9.0f, float(triangles[i].group_id % 10));
                for (int index = 0; index < 3; ++index) {
                    fprintf(fp, "v %f %f %f  %f %f %f\n", mesh.vertices[triangles[i].indices[index]](0),
                            mesh.vertices[triangles[i].indices[index]](1), mesh.vertices[triangles[i].indices[index]](2), color(0),
                            color(1), color(2));
                }
                fprintf(fp, "f %d %d %d\n", i * 3 + 1, i * 3 + 2, i * 3 + 3);
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
                const Triangle& triangle = triangles[triangle_indexes_by_z[i]];
                Vec3f color = value_to_rgbf(0.0f, 9999.0f, float(i % 10000));
                for (int index = 0; index < 3; ++index) {
                    fprintf(fp, "v %f %f %f  %f %f %f\n", mesh.vertices[triangles[i].indices[index]](0),
                            mesh.vertices[triangles[i].indices[index]](1), mesh.vertices[triangles[i].indices[index]](2), color(0),
                            color(1), color(2));
                }
                fprintf(fp, "f %d %d %d\n", i * 3 + 1, i * 3 + 2, i * 3 + 3);
            }
            fclose(fp);
        }
    }
#endif

}
;

inline void do_experimental_support_placement(indexed_triangle_set mesh, TriangleSelectorGUI *selector, float dot_limit) {
    SupportPlacerMesh support_placer { std::move(mesh), dot_limit, 3.0 };

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
