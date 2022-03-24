#include "FDMSupportSpots.hpp"

namespace Slic3r {

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

FDMSupportSpots::FDMSupportSpots(FDMSupportSpotsConfig config, indexed_triangle_set mesh, const Transform3d &transform) :
        m_config(config) {
    using namespace FDMSupportSpotsImpl;

    // Transform and subdivide first
    its_transform(mesh, transform);
//    m_mesh = its_subdivide(mesh, config.max_side_length);
    m_mesh = mesh;

    // Prepare data structures

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

        Triangle t { };
        t.indices = mesh.indices[face_index];
        t.normal = normal;
        t.center = (vertices[0] + vertices[1] + vertices[2]) / 3.0f;
        t.downward_dot_value = normal.dot(down);
        t.index = face_index;
        t.neighbours = neighbours[face_index];
        t.lowest_z_coord = vertices[0].z();
        t.edge_dot_value = std::max(lowest_edge_a.dot(down), lowest_edge_b.dot(down));
        t.area = its_face_area(mesh, face_index);

        this->m_triangles.push_back(t);
        this->m_triangle_indexes_by_z.push_back(face_index);
    }

    assert(this->m_triangle_indexes_by_z.size() == this->m_triangles.size());
    assert(mesh.indices.size() == this->m_triangles.size());

    std::sort(this->m_triangle_indexes_by_z.begin(), this->m_triangle_indexes_by_z.end(),
            [&](const size_t &left, const size_t &right) {
                if (this->m_triangles[left].lowest_z_coord != this->m_triangles[right].lowest_z_coord) {
                    return this->m_triangles[left].lowest_z_coord < this->m_triangles[right].lowest_z_coord;
                } else if (this->m_triangles[left].edge_dot_value != this->m_triangles[right].edge_dot_value) {
                    return this->m_triangles[left].edge_dot_value > this->m_triangles[right].edge_dot_value;
                } else {
                    return (abs(this->m_triangles[left].center.x() + this->m_triangles[left].center.y()) <
                            abs(this->m_triangles[right].center.x() + this->m_triangles[right].center.y()));
                }
            });

    for (size_t order_index = 0; order_index < this->m_triangle_indexes_by_z.size(); ++order_index) {
        this->m_triangles[this->m_triangle_indexes_by_z[order_index]].order_by_z = order_index;
    }

    for (Triangle &triangle : this->m_triangles) {
        std::sort(begin(triangle.neighbours), end(triangle.neighbours), [&](const int left, const int right) {
            if (left < 0) {
                return false;
            }
            if (right < 0) {
                return true;
            }
            return this->m_triangles[left].order_by_z < this->m_triangles[right].order_by_z;
        });
    }
}

float FDMSupportSpots::triangle_vertices_shortest_distance(const indexed_triangle_set &its, const size_t &face_a,
        const size_t &face_b) const {
    float distance = std::numeric_limits<float>::max();
    for (const auto &vertex_a_index : its.indices[face_a]) {
        for (const auto &vertex_b_index : its.indices[face_b]) {
            distance = std::min(distance, (its.vertices[vertex_a_index] - its.vertices[vertex_b_index]).norm());
        }
    }
    return distance;
}

void FDMSupportSpots::find_support_areas() {
    using namespace FDMSupportSpotsImpl;
    size_t next_group_id = 1;
    for (const size_t &current_index : this->m_triangle_indexes_by_z) {
        Triangle &current = this->m_triangles[current_index];

        size_t group_id = 0;
        float neighbourhood_unsupported_area = this->m_config.patch_spacing;
        bool visited_neighbour = false;

        std::queue<int> neighbours { };
        neighbours.push(current_index);
        std::set<int> explored { };
        while (!neighbours.empty() && neighbourhood_unsupported_area > 0) {
            int neighbour_index = neighbours.front();
            neighbours.pop();

            const Triangle &neighbour = this->m_triangles[neighbour_index];
            if (explored.find(neighbour_index) != explored.end()) {
                continue;
            }
            explored.insert(neighbour_index);
            if (triangle_vertices_shortest_distance(this->m_mesh, current.index, neighbour_index)
                    > this->m_config.islands_tolerance_distance) {
                continue;
            }

            if (neighbour.visited) {
                visited_neighbour = true;
                if (neighbourhood_unsupported_area >= neighbour.unsupported_weight) {
                    neighbourhood_unsupported_area = neighbour.unsupported_weight;
                    group_id = neighbour.group_id;
                }
                break;
            }
            for (const auto &neighbour_index : neighbour.neighbours) {
                if (neighbour_index >= 0) {
                    neighbours.push(neighbour_index);
                }
            }
        }

        current.visited = true;
        current.unsupported_weight =
                current.downward_dot_value >= this->m_config.limit_angle_cos ?
                        current.area + neighbourhood_unsupported_area : 0;
        current.group_id = group_id;

        if (current.downward_dot_value > 0
                && (current.unsupported_weight > this->m_config.patch_spacing || !visited_neighbour)) {
            group_id = next_group_id;
            next_group_id++;

            std::queue<int> supporters { };
            current.visited = false;
            supporters.push(int(current_index));
            float supported_size = 0;
            while (supported_size < this->m_config.patch_size && !supporters.empty()) {
                int s = supporters.front();
                supporters.pop();
                Triangle &supporter = this->m_triangles[s];
                if (supporter.downward_dot_value <= 0.1) {
                    continue;
                }

                if (supporter.visited) {
                    supported_size += supporter.supports ? supporter.area : 0;
                } else {
                    supporter.supports = true;
                    supporter.unsupported_weight = 0;
                    supported_size += supporter.area;
                    supporter.visited = true;
                    supporter.group_id = group_id;
                    for (const auto &n : supporter.neighbours) {
                        if (n < 0)
                            continue;
                        supporters.push(n);
                    }
                }
            }
        }

    }
}

#ifdef DEBUG_FILES
void FDMSupportSpots::debug_export() const {
    using namespace FDMSupportSpotsImpl;
    Slic3r::CNumericLocalesSetter locales_setter;
    {
        std::string file_name = debug_out_path("groups.obj");
        FILE *fp = boost::nowide::fopen(file_name.c_str(), "w");
        if (fp == nullptr) {
            BOOST_LOG_TRIVIAL(error)
            << "stl_write_obj: Couldn't open " << file_name << " for writing";
            return;
        }

        for (size_t i = 0; i < this->m_triangles.size(); ++i) {
            Vec3f color = value_to_rgbf(0.0f, 19.0f, float(this->m_triangles[i].group_id % 20));
            for (size_t index = 0; index < 3; ++index) {
                fprintf(fp, "v %f %f %f  %f %f %f\n", this->m_mesh.vertices[this->m_triangles[i].indices[index]](0),
                        this->m_mesh.vertices[this->m_triangles[i].indices[index]](1),
                        this->m_mesh.vertices[this->m_triangles[i].indices[index]](2), color(0),
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

        for (size_t i = 0; i < this->m_triangle_indexes_by_z.size(); ++i) {
            const Triangle &triangle = this->m_triangles[this->m_triangle_indexes_by_z[i]];
            Vec3f color = Vec3f { float(i) / float(this->m_triangle_indexes_by_z.size()), float(i)
                    / float(this->m_triangle_indexes_by_z.size()), 0.5 };
            for (size_t index = 0; index < 3; ++index) {
                fprintf(fp, "v %f %f %f  %f %f %f\n", this->m_mesh.vertices[triangle.indices[index]](0),
                        this->m_mesh.vertices[triangle.indices[index]](1),
                        this->m_mesh.vertices[triangle.indices[index]](2), color(0),
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

        for (size_t i = 0; i < this->m_triangle_indexes_by_z.size(); ++i) {
            const Triangle &triangle = this->m_triangles[this->m_triangle_indexes_by_z[i]];
            Vec3f color = value_to_rgbf(0, this->m_config.patch_size, triangle.unsupported_weight);
            for (size_t index = 0; index < 3; ++index) {
                fprintf(fp, "v %f %f %f  %f %f %f\n", this->m_mesh.vertices[triangle.indices[index]](0),
                        this->m_mesh.vertices[triangle.indices[index]](1),
                        this->m_mesh.vertices[triangle.indices[index]](2), color(0),
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

        for (size_t i = 0; i < this->m_triangle_indexes_by_z.size(); ++i) {
            const Triangle &triangle = this->m_triangles[this->m_triangle_indexes_by_z[i]];
            Vec3f color = value_to_rgbf(-1, 1, triangle.downward_dot_value);
            for (size_t index = 0; index < 3; ++index) {
                fprintf(fp, "v %f %f %f  %f %f %f\n", this->m_mesh.vertices[triangle.indices[index]](0),
                        this->m_mesh.vertices[triangle.indices[index]](1),
                        this->m_mesh.vertices[triangle.indices[index]](2), color(0),
                        color(1), color(2));
            }
            fprintf(fp, "f %zu %zu %zu\n", i * 3 + 1, i * 3 + 2, i * 3 + 3);
        }
        fclose(fp);
    }
}
#endif

}

