#ifndef slic3r_ZDither_hpp
#define slic3r_Zdither_hpp

#include "Point.hpp"
#include "TriangleMeshSlicer.hpp"

namespace Slic3r {

void midslice_zs(const indexed_triangle_set &mesh,
                 const std::vector<float> &  zs,
                 const Transform3d &         trafo,
                 float                       nozzle_diameter,
                 std::vector<float> *        mid_zs,
                 std::vector<int> *          upwrd_mididx,
                 std::vector<int> *          dnwrd_mididx);

std::vector<ExPolygons5> apply_z_dither(const std::vector<ExPolygons> &layers,
                                        const std::vector<ExPolygons> &mid_layers,
                                        const std::vector<bool> &do_low,
                                        const std::vector<bool> &do_high);

std::vector<ExPolygons5> z_dither(const indexed_triangle_set &   mesh,
                                  const std::vector<float> &     zs,
                                  const MeshSlicingParamsEx &    params,
                                  const std::vector<ExPolygons> &layers,
                                  const std::function<void()> &  throw_on_cancel_callback);

} // namespace Slic3r
#endif
