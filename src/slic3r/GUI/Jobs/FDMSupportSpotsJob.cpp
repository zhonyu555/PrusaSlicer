#include "FDMSupportSpotsJob.hpp"
#include "libslic3r/TriangleSelector.hpp"

#include "slic3r/GUI/GLCanvas3D.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/Gizmos/GLGizmoFdmSupports.hpp"

namespace Slic3r::GUI {

FDMSupportSpotsJob::FDMSupportSpotsJob(Plater *plater,
        FDMSupportSpotsConfig support_spots_config,
        ObjectID model_object_id,
        ObjectID model_instance_id,
        std::unordered_map<size_t, SupportedMeshData> model_volumes_data) :
        m_plater(plater), m_support_spots_config(support_spots_config), m_model_object_id(model_object_id), m_model_instance_id(
                model_instance_id),
                m_model_volumes_data(model_volumes_data)
{
}

void FDMSupportSpotsJob::process(Ctl &ctl) {
    if (ctl.was_canceled()) {
        return;
    }

    auto status_text_preparing = _u8L("Densifying mesh and preparing search structures");
    auto status_text_computing = _u8L("Computing support placement");
    auto status_text_canceled = _u8L("Support placement computation canceled");
    auto status_text_done = _u8L("Support placement computation finished");

    int step_size = 100 / (this->m_model_volumes_data.size() * 2);
    int status = 0;
    for (auto &data : this->m_model_volumes_data) {
        if (ctl.was_canceled()) {
            ctl.update_status(100, status_text_canceled);
            return;
        }
        ctl.update_status(status, status_text_preparing);
        status += step_size;
        FDMSupportSpots support_spots_alg { m_support_spots_config, data.second.mesh, data.second.transform };
        if (ctl.was_canceled()) {
            ctl.update_status(100, status_text_canceled);
            return;
        }
        ctl.update_status(status, status_text_computing);
        status += step_size;
        support_spots_alg.find_support_areas();
        std::vector<size_t> supported_face_indexes { };
        for (const auto &triangle : support_spots_alg.m_triangles) {
            if (triangle.supports) {
                supported_face_indexes.push_back(triangle.index);
            }
        }

        support_spots_alg.debug_export();

        this->m_computed_support_data.emplace(data.first, supported_face_indexes);
    }

    if (ctl.was_canceled()) {
        ctl.update_status(100, status_text_canceled);
        return;
    }

    ctl.update_status(100, status_text_canceled);
}

void FDMSupportSpotsJob::finalize(bool canceled, std::exception_ptr &exception) {
    if (canceled || exception)
        return;

    ModelObject *model_object = nullptr;
    for (ModelObject *mo : this->m_plater->model().objects) {
        if (mo->id() == this->m_model_object_id) {
            model_object = mo;
            break;
        }
    }

    if (model_object == nullptr)
        return;

    ModelInstance *model_instance = nullptr;
    for (ModelInstance *mi : model_object->instances) {
        if (mi->id() == this->m_model_instance_id) {
            model_instance = mi;
            break;
        }
    }
    if (model_instance == nullptr)
        return;

    for (ModelVolume *mv : model_object->volumes) {
        auto fdm_support_spots_result = this->m_computed_support_data.find(mv->id().id);
        if (fdm_support_spots_result != this->m_computed_support_data.end()) {
            TriangleSelector selector { mv->mesh() };
            for (size_t face_index : fdm_support_spots_result->second) {
                selector.set_facet(face_index, EnforcerBlockerType::ENFORCER);
            }
            mv->supported_facets.set(selector);
        }
    }
}

}
