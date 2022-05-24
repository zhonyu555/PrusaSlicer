#include "FixModelByRaycasts.hpp"
#include "libslic3r/AABBTreeIndirect.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/Model.hpp"
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"
#include "libigl/igl/copyleft/marching_cubes.h"
#include "libigl/igl/voxel_grid.h"

namespace Slic3r {

namespace detail {

Vec3f sample_sphere_uniform(const Vec2f &samples) {
    float term1 = 2.0f * float(PI) * samples.x();
    float term2 = 2.0f * sqrt(samples.y() - samples.y() * samples.y());
    return {cos(term1) * term2, sin(term1) * term2,
        1.0f - 2.0f * samples.y()};
}

float sigmoid(float x) {
    float e = std::exp(x);
    return (2.0f * e) / (1.0f + e) - 1.0f;
}

indexed_triangle_set fix_model_volume_mesh(const TriangleMesh &mesh) {
    float thickness = 2.0f;
    float resolution = 0.3f;

    //prepare uniform samples of a sphere
    size_t sqrt_sample_count = 8;
    float step_size = 1.0f / sqrt_sample_count;
    std::vector<Vec3f> precomputed_sample_directions(
            sqrt_sample_count * sqrt_sample_count);
    for (size_t x_idx = 0; x_idx < sqrt_sample_count; ++x_idx) {
        float sample_x = x_idx * step_size + step_size / 2.0;
        for (size_t y_idx = 0; y_idx < sqrt_sample_count; ++y_idx) {
            size_t dir_index = x_idx * sqrt_sample_count + y_idx;
            float sample_y = y_idx * step_size + step_size / 2.0;
            precomputed_sample_directions[dir_index] = sample_sphere_uniform( { sample_x, sample_y });
        }
    }

    indexed_triangle_set its = mesh.its;
    float max_size = mesh.bounding_box().size().maxCoeff();
    int samples = max_size / resolution;
    // create grid
    Eigen::MatrixXf grid_points;
    Eigen::RowVector3i res;

    const BoundingBoxf3 &slicer_bbox = mesh.bounding_box();
    Eigen::AlignedBox<float, 3> eigen_box(slicer_bbox.min.cast<float>(), slicer_bbox.max.cast<float>());

    std::cout << "building voxel grid " << std::endl;
    igl::voxel_grid(eigen_box, samples, 1, grid_points, res);

    auto tree = AABBTreeIndirect::build_aabb_tree_over_indexed_triangle_set(its.vertices, its.indices);
    Eigen::VectorXf grid_values;
    grid_values.resize(grid_points.size());

    std::cout << "g info: " << grid_points.size() << std::endl;
    std::cout << "g info: " << grid_points.rows() << std::endl;
    std::cout << "g info: " << grid_points.cols() << std::endl;

    std::cout << "raycasting " << std::endl;

    tbb::parallel_for(
            tbb::blocked_range<size_t>(0, grid_points.rows()), [&](tbb::blocked_range<size_t> r) {
                for (size_t index = r.begin(); index < r.end(); ++index) {
                    Vec3f origin = grid_points.row(index);
                    float &value = grid_values(index);

                    size_t hit_idx;
                    Vec3f hit_point;
                    bool apply_bonus = false;
                    float distance = sqrt(AABBTreeIndirect::squared_distance_to_indexed_triangle_set(its.vertices, its.indices, tree, origin, hit_idx, hit_point));
                    Vec3f face_normal = its_face_normal(its, hit_idx);
                    if ((hit_point - origin).dot(face_normal) > 0 && distance < thickness) {
                        apply_bonus = true;
                    }

                    std::vector<igl::Hit> hits;
                    size_t inside_hits = 0;
                    for (const Vec3f &dir : precomputed_sample_directions) {
                        if (AABBTreeIndirect::intersect_ray_all_hits(its.vertices, its.indices, tree,
                                Vec3d(origin.cast<double>()),
                                Vec3d(dir.cast<double>()),
                                hits)) {
                            for (const auto &hit : hits) {
                                Vec3f face_normal = its_face_normal(its, hit.id);
                                auto dot = dir.dot(face_normal);
                                if (dot > 0) {
                                    inside_hits += 1;
                                    break;
                                } else if (hit.t > thickness) {
                                    break;
                                }
                            }
                        }
                    }

                    float ratio = float(inside_hits) / float(precomputed_sample_directions.size());
                    float x = sigmoid(6 * (-ratio + 0.666f));

                    if (x < 0 && apply_bonus) {
                        x = x * 2.0f;
                    }
                    value = distance * x;
                }
            }
            );

    std::cout << "marching cubes " << std::endl;
    Eigen::MatrixXf vertices;
    Eigen::MatrixXi faces;
    igl::copyleft::marching_cubes(grid_values, grid_points, res.x(), res.y(), res.z(), vertices, faces);

    std::cout << "vertices info: " << vertices.size() << std::endl;
    std::cout << "vertices info: " << vertices.rows() << std::endl;
    std::cout << "vertices info: " << vertices.cols() << std::endl;

    indexed_triangle_set fixed_mesh;
    fixed_mesh.vertices.resize(vertices.rows());
    fixed_mesh.indices.resize(faces.rows());

    for (int v = 0; v < vertices.rows(); ++v) {
        fixed_mesh.vertices[v] = vertices.row(v);
    }

    for (int f = 0; f < faces.rows(); ++f) {
        fixed_mesh.indices[f] = faces.row(f);
    }

    return fixed_mesh;
}

}

bool fix_model_by_raycasts(ModelObject &model_object, int volume_idx, wxProgressDialog &progress_dlg,
        const wxString &msg_header, std::string &fix_result) {

    std::vector<ModelVolume*> volumes;
    if (volume_idx == -1) {
        volumes = model_object.volumes;
    } else {
        volumes.emplace_back(model_object.volumes[volume_idx]);
    }

    for (ModelVolume *mv : volumes) {
        auto mesh = mv->mesh();
        mv->set_mesh(detail::fix_model_volume_mesh(mesh));
        std::cout << "update mv " << std::endl;
        mv->calculate_convex_hull();
        mv->set_new_unique_id();
    }
    model_object.invalidate_bounding_box();

    return true;
}

}
