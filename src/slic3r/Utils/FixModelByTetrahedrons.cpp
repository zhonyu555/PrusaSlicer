#include "FixModelByRaycasts.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/Model.hpp"

#include "libigl/igl/copyleft/marching_cubes.h"
#include "libigl/igl/voxel_grid.h"
#include "libigl/igl/barycenter.h"
#include "libigl/igl/remove_unreferenced.h"
#include "libigl/igl/copyleft/cgal/remesh_self_intersections.h"
#include "libigl/igl/winding_number.h"
#include "libigl/igl/boundary_facets.h"
#include "libigl/igl/copyleft/cgal/outer_hull.h"
#include "libigl/igl/copyleft/tetgen/tetrahedralize.h"

#include <algorithm>

namespace Slic3r {

namespace detail {

indexed_triangle_set fix_model_volume_mesh(const TriangleMesh &mesh) {

    //first compute convex hull
    Eigen::MatrixXd hull_v;
    Eigen::MatrixXi hull_f;
    {
        Eigen::MatrixXf vertices(mesh.its.vertices.size(), 3);
        Eigen::MatrixXi faces(mesh.its.indices.size(), 3);

        for (int v = 0; v < mesh.its.vertices.size(); ++v) {
            vertices.row(v) = mesh.its.vertices[v];
        }

        for (int v = 0; v < mesh.its.indices.size(); ++v) {
            faces.row(v) = mesh.its.indices[v];
        }

        Eigen::VectorXi J;
        Eigen::VectorXi flip;

        std::cout << "vertices v" << vertices.rows() << std::endl;
        std::cout << "faces f" << faces.rows() << std::endl;

        igl::copyleft::cgal::outer_hull(vertices, faces, hull_v, hull_f, J, flip);

        std::cout << "hull v" << hull_v.rows() << std::endl;
        std::cout << "hull f" << hull_f.rows() << std::endl;
    }

    // then tetrahedronize the convex hull
    Eigen::MatrixXf tets_v;
    Eigen::MatrixXi tets_t;
    Eigen::MatrixXi tets_f;
    int result = igl::copyleft::tetgen::tetrahedralize(hull_v, hull_f, "pA", tets_v, tets_t, tets_f);
    if (result != 0) {
        std::cout << "Tetrahedronization failed " << std::endl;
        return mesh.its;
    }

    Eigen::MatrixXd barycenters;
    // Compute barycenters of all tets
    std::cout << "Computing barucenters " << std::endl;
    igl::barycenter(hull_v, tets_t, barycenters);

    // Compute generalized winding number at all barycenters
    std::cout << "Computing winding number over all " << tets_t.rows() << " tets..." << std::endl;
    Eigen::VectorXd W;
    igl::winding_number(hull_v, hull_f, barycenters, W);

    // Extract interior tets
    Eigen::MatrixXi CT((W.array() > 0.5).count(), 4);
    {
        size_t k = 0;
        for (size_t t = 0; t < tets_t.rows(); t++)
                {
            if (W(t) > 0.5)
                    {
                CT.row(k) = tets_t.row(t);
                k++;
            }
        }
    }

    Eigen::MatrixXi new_faces;
    // find bounary facets of interior tets
    igl::boundary_facets(tets_t, new_faces);
    // boundary_facets seems to be reversed...
    new_faces = new_faces.rowwise().reverse().eval();

    std::cout << "new faces: " << new_faces.rows() << std::endl;

    indexed_triangle_set fixed_mesh;
    fixed_mesh.vertices.resize(hull_v.rows());
    fixed_mesh.indices.resize(hull_f.rows());

    for (int v = 0; v < hull_v.rows(); ++v) {
        fixed_mesh.vertices[v] = hull_v.row(v).cast<float>();
    }

    for (int f = 0; f < hull_f.rows(); ++f) {
        fixed_mesh.indices[f] = hull_f.row(f);
    }

    std::cout << "returning fixed mesh " << std::endl;

    return fixed_mesh;
}

}

bool fix_model_by_tetrahedrons(ModelObject &model_object, int volume_idx, wxProgressDialog &progress_dlg,
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
