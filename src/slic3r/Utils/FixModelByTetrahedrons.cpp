#include "FixModelByTetrahedrons.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/Model.hpp"

#include "libigl/igl/copyleft/marching_cubes.h"
#include "libigl/igl/voxel_grid.h"
#include "libigl/igl/for_each.h"
#include "libigl/igl/barycenter.h"
#include "libigl/igl/unique_simplices.h"
#include "libigl/igl/remove_duplicates.h"
#include "libigl/igl/remove_unreferenced.h"
#include "libigl/igl/copyleft/cgal/remesh_self_intersections.h"
#include "libigl/igl/winding_number.h"
#include "libigl/igl/boundary_facets.h"
#include "libigl/igl/copyleft/cgal/convex_hull.h"
#include "libigl/igl/copyleft/cgal/outer_hull.h"
#include "libigl/igl/copyleft/tetgen/tetrahedralize.h"

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

bool fix_mesh(
        const Eigen::MatrixXd &V,
        const Eigen::MatrixXi &F,
        Eigen::MatrixXd &NV,
        Eigen::MatrixXi &NF) {

    NV = V;
    NF = F;

    {
        std::cout << "remesh " << std::endl;
        Eigen::MatrixXd OV = NV;
        Eigen::MatrixXi OF = NF;
        Eigen::MatrixXi _TMP1;
        Eigen::VectorXi _TMP2;
        Eigen::VectorXi IM;
        igl::copyleft::cgal::remesh_self_intersections(OV, OF, { }, NV, NF, _TMP1, _TMP2, IM);
        // _apply_ duplicate vertex mapping IM to FF
        for (int i = 0; i < NF.size(); ++i) {
            NF.data()[i] = IM(NF.data()[i]);
        }
    }
    {
        std::cout << "duplicates " << std::endl;
        Eigen::MatrixXd OV = NV;
        Eigen::MatrixXi OF = NF;
        Eigen::VectorXi IM;
        igl::remove_duplicates(OV, OF, NV, NF, IM, 0.01);
    }
    {
        std::cout << "unique triangles " << std::endl;
        Eigen::MatrixXi oldF = NF;
        igl::unique_simplices(oldF, NF);
    }
    {
        std::cout << "remove unreferenced " << std::endl;
        Eigen::MatrixXd oldRV = NV;
        Eigen::MatrixXi oldRF = NF;
        Eigen::VectorXi IM;
        igl::remove_unreferenced(oldRV, oldRF, NV, NF, IM);
    }

    std::cout << "tetrahedronize " << std::endl;
    Eigen::MatrixXd tets_v;
    Eigen::MatrixXi tets_t;
    Eigen::MatrixXi tets_f;
    int result = igl::copyleft::tetgen::tetrahedralize(NV, NF, "cY", tets_v, tets_t, tets_f);
    if (result != 0) {
        return false;
    }

    std::cout << "barycenters " << std::endl;
    Eigen::MatrixXd barycenters;
    igl::barycenter(tets_v, tets_t, barycenters);

    std::cout << "winding number " << std::endl;
    Eigen::VectorXd W;
    igl::winding_number(V, F, barycenters, W);
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
    std::cout << "boundary facets " << std::endl;
    igl::boundary_facets(CT, NF);
    NF = NF.rowwise().reverse().eval();

    {
        std::cout << "remove unreferenced " << std::endl;
        Eigen::MatrixXi oldRF = NF;
        Eigen::VectorXi IM;
        igl::remove_unreferenced(tets_v, oldRF, NV, NF, IM);
    }

    return true;

}

indexed_triangle_set fix_model_volume_mesh(const indexed_triangle_set &mesh) {
    //first compute convex hull
    Eigen::MatrixXd vertices;
    Eigen::MatrixXi faces;

    Eigen::MatrixXd orig_v(mesh.vertices.size(), 3);
    Eigen::MatrixXi orig_f(mesh.indices.size(), 3);

    for (int v = 0; v < mesh.vertices.size(); ++v) {
        orig_v.row(v) = mesh.vertices[v].cast<double>();
    }

    for (int v = 0; v < mesh.indices.size(); ++v) {
        orig_f.row(v) = mesh.indices[v];
    }

    fix_mesh(orig_v, orig_f, vertices, faces);

    indexed_triangle_set fixed_mesh;
    fixed_mesh.vertices.resize(vertices.rows());
    fixed_mesh.indices.resize(faces.rows());

    for (int v = 0; v < vertices.rows(); ++v) {
        fixed_mesh.vertices[v] = vertices.row(v).cast<float>();
    }

    for (int f = 0; f < faces.rows(); ++f) {
        fixed_mesh.indices[f] = faces.row(f);
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
