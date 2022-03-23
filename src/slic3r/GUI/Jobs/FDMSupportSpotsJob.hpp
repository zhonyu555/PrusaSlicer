#ifndef SRC_SLIC3R_GUI_JOBS_FDMSUPPORTSPOTSJOB_HPP_
#define SRC_SLIC3R_GUI_JOBS_FDMSUPPORTSPOTSJOB_HPP_

#include "Job.hpp"
#include "libslic3r/FDMSupportSpots.hpp"
#include "slic3r/GUI/Plater.hpp"

namespace Slic3r::GUI {

struct SupportedMeshData {
    indexed_triangle_set mesh;
    Transform3d transform;
};

class FDMSupportSpotsJob: public Job {
    Plater *m_plater;
    FDMSupportSpotsConfig m_support_spots_config;
    ObjectID m_model_object_id;
    ObjectID m_model_instance_id;
    //map of model volume ids and corresponding mesh data
    std::unordered_map<size_t, SupportedMeshData> m_model_volumes_data;

    //vector of supported face indexes for each model volume
    std::unordered_map<size_t, std::vector<size_t>> m_computed_support_data;

public:
    FDMSupportSpotsJob(Plater *plater,
            FDMSupportSpotsConfig support_spots_config,
            ObjectID model_object_id,
            ObjectID model_instance_id,
            std::unordered_map<size_t, SupportedMeshData> model_volumes_data);

    void process(Ctl &ctl) override;

    void finalize(bool canceled, std::exception_ptr &exception) override;

};

}

#endif /* SRC_SLIC3R_GUI_JOBS_FDMSUPPORTSPOTSJOB_HPP_ */
