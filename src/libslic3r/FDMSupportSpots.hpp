#ifndef SRC_LIBSLIC3R_FDMSUPPORTSPOTS_HPP_
#define SRC_LIBSLIC3R_FDMSUPPORTSPOTS_HPP_

#include "libslic3r/Model.hpp"

#include <algorithm>
#include <queue>

#include <boost/nowide/cstdio.hpp>
#include <boost/log/trivial.hpp>

#define DEBUG_FILES

#ifdef DEBUG_FILES
#include <boost/nowide/cstdio.hpp>
#include "libslic3r/Utils.hpp"
#endif

namespace Slic3r {

namespace FDMSupportSpotsImpl {
struct Triangle {
    stl_triangle_vertex_indices indices;
    Vec3f normal;
    float downward_dot_value;
    size_t index;
    Vec3i neighbours;
    float lowest_z_coord;
    // higher value of dot product of the downward direction and the two bottom edges
    float edge_dot_value;
    float area;

    size_t order_by_z;

    //members updated during algorithm
    float unsupported_weight { 0.0 };
    bool supports = false;
    bool visited = false;
    size_t group_id = 0;
};
}

struct FDMSupportSpotsConfig {
    float limit_angle_cos { 35.0f * PI / 180.0f };
    float patch_size { 6.0f };
    float patch_spacing { 6.0f };
    float islands_tolerance_distance { 1.0f };
    float max_side_length { 1.0f };
};

struct FDMSupportSpots {
    FDMSupportSpotsConfig m_config;
    indexed_triangle_set m_mesh;
    std::vector<FDMSupportSpotsImpl::Triangle> m_triangles;
    std::vector<size_t> m_triangle_indexes_by_z;

    explicit FDMSupportSpots(FDMSupportSpotsConfig config, indexed_triangle_set mesh, const Transform3d& transform);

    float triangle_vertices_shortest_distance(const indexed_triangle_set &its, const size_t &face_a,
            const size_t &face_b) const;

    void find_support_areas();

#ifdef DEBUG_FILES
    void debug_export() const;
#endif

}
;

}

#endif /* SRC_LIBSLIC3R_FDMSUPPORTSPOTS_HPP_ */
