#include "FixModelByTetrahedrons.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/Model.hpp"

#include "libigl/igl/copyleft/marching_cubes.h"
#include "libigl/igl/voxel_grid.h"
#include "libigl/igl/barycenter.h"
#include "libigl/igl/remove_unreferenced.h"
#include "libigl/igl/copyleft/cgal/remesh_self_intersections.h"
#include "libigl/igl/winding_number.h"
#include "libigl/igl/boundary_facets.h"
#include "libigl/igl/copyleft/cgal/convex_hull.h"
#include "libigl/igl/copyleft/tetgen/tetrahedralize.h"
#include "libigl/igl/copyleft/tetgen/cdt.h"

#include <algorithm>
#include <string.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <exception>
#include <string>
#include <thread>

#include <boost/filesystem.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/nowide/cstdio.hpp>
#include <boost/thread.hpp>

#include "../GUI/GUI.hpp"
#include "../GUI/I18N.hpp"
#include "../GUI/MsgDialog.hpp"

#include <wx/msgdlg.h>
#include <wx/progdlg.h>


namespace Slic3r {

class RepairCanceledException: public std::exception {
public:
    const char* what() const throw () {
        return "Model repair has been cancelled";
    }
};

class RepairFailedException: public std::exception {
public:
    const char* what() const throw () {
        return "Model repair has failed";
    }
};


namespace detail {

indexed_triangle_set fix_model_volume_mesh(const indexed_triangle_set &mesh) {
    //first compute convex hull
    Eigen::MatrixXd vertices;
    Eigen::MatrixXi faces;
    Eigen::MatrixXi hull_faces;
    {
        Eigen::MatrixXf orig_v(mesh.vertices.size(), 3);
        Eigen::MatrixXi orig_f(mesh.indices.size(), 3);

        for (int v = 0; v < mesh.vertices.size(); ++v) {
            orig_v.row(v) = mesh.vertices[v];
        }

        for (int v = 0; v < mesh.indices.size(); ++v) {
            orig_f.row(v) = mesh.indices[v];
        }

        std::cout << "orig vertices: " << orig_v.rows() << std::endl;
        std::cout << "orig faces: " << orig_f.rows() << std::endl;

        Eigen::VectorXi I;
        Eigen::MatrixXi IF;
        Eigen::VectorXi J;
        //resolve self intersections
        igl::copyleft::cgal::remesh_self_intersections(orig_v, orig_f, { }, vertices, faces, IF, J, I);
        std::cout << "remeshed vertices: " << vertices.rows() << std::endl;
        std::cout << "remeshed faces: " << faces.rows() << std::endl;

        // compute hull
        igl::copyleft::cgal::convex_hull(vertices, hull_faces);

        std::cout << " hull faces: " << hull_faces.rows() << std::endl;
    }

    std::cout << "tetrahedronize convex hull " << std::endl;
    Eigen::MatrixXd tets_v;
    Eigen::MatrixXi tets_t;
    Eigen::MatrixXi tets_f;
    int result = igl::copyleft::tetgen::tetrahedralize(vertices, hull_faces, "pq1.414NEF", tets_v, tets_t, tets_f);
    if (result != 0) {
        std::cout << "Tetrahedronization failed " << std::endl;
        indexed_triangle_set fixed_mesh;
        fixed_mesh.vertices.resize(vertices.rows());
        fixed_mesh.indices.resize(hull_faces.rows());

        for (int v = 0; v < vertices.rows(); ++v) {
            fixed_mesh.vertices[v] = vertices.row(v).cast<float>();
        }

        for (int f = 0; f < hull_faces.rows(); ++f) {
            fixed_mesh.indices[f] = hull_faces.row(f);
        }
        return fixed_mesh;
    }

    std::cout << "tetrahedrons count: " << tets_t.rows() << std::endl;

    // Compute barycenters of all tets
    Eigen::MatrixXd barycenters;
    std::cout << "Computing barycenters " << std::endl;
    igl::barycenter(tets_v, tets_t, barycenters);

    std::cout << "barycenters count: " << barycenters.rows() << std::endl;


    // Compute generalized winding number at all barycenters from remeshed input
    std::cout << "Computing winding number over all " << tets_t.rows() << " tets..." << std::endl;
    Eigen::VectorXd W;
    igl::winding_number(vertices, faces, barycenters, W);

    std::cout << "winding numbers count: " << W.rows() << std::endl;



    std::cout << "Extracting internal tetrahedra " << std::endl;
    Eigen::MatrixXi CT((W.array() > 0.5).count(), 4);
    {
        size_t k = 0;
        for (size_t t = 0; t < tets_t.rows(); t++) {
            if (W(t) > 0.5) {
                CT.row(k) = tets_t.row(t);
                k++;
            }
        }
    }

    std::cout << "Extracting boundary faces from  " << CT.rows() << " internal tetrahedra" << std::endl;
    Eigen::MatrixXi new_faces;
    igl::boundary_facets(tets_t, new_faces);
    // boundary_facets seems to be reversed...
    new_faces = new_faces.rowwise().reverse().eval();

    std::cout << "new faces count: " << new_faces.rows() << std::endl;

    indexed_triangle_set fixed_mesh;
    fixed_mesh.vertices.resize(tets_v.rows());
    fixed_mesh.indices.resize(new_faces.rows());

    for (int v = 0; v < tets_v.rows(); ++v) {
        fixed_mesh.vertices[v] = tets_v.row(v).cast<float>();
    }

    for (int f = 0; f < new_faces.rows(); ++f) {
        fixed_mesh.indices[f] = new_faces.row(f);
    }

    std::cout << "returning fixed mesh " << std::endl;

    return fixed_mesh;
}

}



bool fix_model_by_tetrahedrons(ModelObject &model_object, int volume_idx, wxProgressDialog &progress_dlg,
        const wxString &msg_header, std::string &fix_result) {
    std::mutex mtx;
    std::condition_variable condition;
    struct Progress {
        std::string message;
        int percent = 0;
        bool updated = false;
    } progress;
    std::atomic<bool> canceled = false;
    std::atomic<bool> finished = false;

    std::vector<ModelVolume*> volumes;
    if (volume_idx == -1)
        volumes = model_object.volumes;
    else
        volumes.emplace_back(model_object.volumes[volume_idx]);

    // Executing the calculation in a background thread, so that the COM context could be created with its own threading model.
    // (It seems like wxWidgets initialize the COM contex as single threaded and we need a multi-threaded context).
    bool success = false;
    size_t ivolume = 0;
    auto on_progress = [&mtx, &condition, &ivolume, &volumes, &progress](const char *msg, unsigned prcnt) {
        std::unique_lock<std::mutex> lock(mtx);
        progress.message = msg;
        progress.percent = (int) floor((float(prcnt) + float(ivolume) * 100.f) / float(volumes.size()));
        progress.updated = true;
        condition.notify_all();
    };
    auto worker_thread = boost::thread(
            [&model_object, &volumes, &ivolume, on_progress, &success, &canceled, &finished]() {
                try {
                    std::vector<TriangleMesh> meshes_repaired;
                    meshes_repaired.reserve(volumes.size());
                    for (ModelVolume *mv : volumes) {
                        meshes_repaired.emplace_back(detail::fix_model_volume_mesh(mv->mesh().its));
                    }

                    for (size_t i = 0; i < volumes.size(); ++i) {
                        volumes[i]->set_mesh(std::move(meshes_repaired[i]));
                        volumes[i]->calculate_convex_hull();
                        volumes[i]->set_new_unique_id();
                    }
                    model_object.invalidate_bounding_box();
                    --ivolume;
                    on_progress(L("Model repair finished"), 100);
                    success = true;
                    finished = true;
                } catch (RepairCanceledException& /* ex */) {
                    canceled = true;
                    finished = true;
                    on_progress(L("Model repair canceled"), 100);
                } catch (std::exception &ex) {
                    success = false;
                    finished = true;
                    on_progress(ex.what(), 100);
                }
            });

    while (!finished) {
        std::unique_lock<std::mutex> lock(mtx);
        condition.wait_for(lock, std::chrono::milliseconds(250), [&progress] {
            return progress.updated;
        });
        // decrease progress.percent value to avoid closing of the progress dialog
        if (!progress_dlg.Update(progress.percent - 1, msg_header + _(progress.message)))
            canceled = true;
        else
            progress_dlg.Fit();
        progress.updated = false;
    }

    worker_thread.join();

    if (canceled) {
        // Nothing to show.
    } else if (success) {
        fix_result = "";
    } else {
        fix_result = progress.message;
    }

    return !canceled;
}

}
