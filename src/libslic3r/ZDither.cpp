#include "ZDither.hpp"
#include "ExPolygon.hpp"
#include "TriangleMeshSlicer.hpp"


namespace Slic3r {

// Find z where additional midlevel slices need to be made for z-dithering
// It is assumed that original layers corresponding to zs[i] will be filled
// from zs[i] - layerHight/2 till zs[i] + layerHeight/2
// Required midlevel slices are to be computed halfway between layers,
// starting with top of layer 0 and ending with top of last layer ;

void midslice_zs(const indexed_triangle_set &mesh,
                 const std::vector<float> &  zs,
                 const Transform3d &         trafo,
                 float                       nozzle_diameter,
                 std::vector<float> *        mid_zs,
                 std::vector<int> *         upwrd_mididx,
                 std::vector<int> *         dnwrd_mididx)
{
    mid_zs->clear();
    upwrd_mididx->clear();
    dnwrd_mididx->clear();

    auto n_zs = zs.size();
    if (n_zs < 2) return;

    std::vector<stl_vertex> transformed_vertices(mesh.vertices);
    auto                    trafo_float = trafo.cast<float>();
    for (stl_vertex &v : transformed_vertices) { v = trafo_float * v; }

    std::vector<float> candidate_zs(n_zs);
    std::vector<float> layerHeight(n_zs);
    float              layerHeightMax = 0;

    for (auto i = 0; i < n_zs - 1; i++) {
        candidate_zs[i] = (zs[i + 1] + zs[i]) / 2;
        layerHeight[i]  = zs[i + 1] - zs[i];
        layerHeightMax  = std::max(layerHeight[i], layerHeightMax);
    }
    layerHeight[n_zs - 1]  = layerHeight[n_zs - 2];
    candidate_zs[n_zs - 1] = zs[n_zs - 1] + layerHeight[n_zs - 1] / 2;

    upwrd_mididx->assign(n_zs, -1);
    dnwrd_mididx->assign(n_zs, -1);

    float nDia = 1.0; // Cooficient to avoid cutting layers with too vertical triangles (see below)
    // With nozzle_diameter = 0.4 and layer_height = 0.25
    // nDia = 1.5 results in slopes below 22.5 degrees; nDia = 1 results in slopes under 32 degrees
    // nDia = 0.7 - 42 degrees; nDia = 0.5 51.4 degrees

    float eps  = layerHeightMax / 100;

    for (auto i = 0; i < mesh.indices.size(); ++i) {
        const stl_triangle_vertex_indices &facetVertices = mesh.indices[i];
        stl_vertex vertex[3] = {transformed_vertices[facetVertices[0]], transformed_vertices[facetVertices[1]],
                                transformed_vertices[facetVertices[2]]};

        Vec3f norm((vertex[1] - vertex[0]).cross(vertex[2] - vertex[0]).normalized());
        // Skip triangle if over a thickest layer it would not fit nDia * nozzle_diameter
        if (nozzle_diameter * sqrt(1 - norm[2] * norm[2]) * nDia  > fabs(norm[2]) * layerHeightMax ) continue;

         float z_min = std::min(std::min(vertex[0][2], vertex[1][2]), vertex[2][2]);
         float z_max = std::max(std::max(vertex[0][2], vertex[1][2]), vertex[2][2]);

         int start = std::lower_bound(candidate_zs.begin(), candidate_zs.end(), z_min - eps) - candidate_zs.begin();

         for (auto i = start; i < candidate_zs.size() && candidate_zs[i] < z_max + eps; i++) {
            // Ignore facets that are too vertical to fit nDia nozzles at layer height
            if (nozzle_diameter * sqrt(1 - norm[2] * norm[2]) * nDia < fabs(norm[2]) * layerHeight[i]) {
                if (norm[2] > 0)
                    upwrd_mididx->at(i) = i;
                else
                    dnwrd_mididx->at(i) = i;
            }
        }
    }

    if (std::all_of(upwrd_mididx->begin(), upwrd_mididx->end(), [](int idx) { return idx == -1; }) &&
        std::all_of(dnwrd_mididx->begin(), dnwrd_mididx->end(), [](int idx) { return idx = -1; }))
        return;

    // Retrn mid_zs which will contribute to z-dithering
    for (int i = 0; i < n_zs; i++) {
        if (upwrd_mididx->at(i) + dnwrd_mididx->at(i) != -2) {
            int count = mid_zs->size();
            mid_zs->push_back(candidate_zs[i]);
            if (upwrd_mididx->at(i) != -1) upwrd_mididx->at(i) = count;
            if (dnwrd_mididx->at(i) != -1) dnwrd_mididx->at(i) = count;
        }
    }
    return;
}

std::vector<ExPolygons5> apply_z_dither(const std::vector<ExPolygons> &layers,
                                        const std::vector<ExPolygons> &layers_mid,
                                        const std::vector<int> &       upwrd_mididx,
                                        const std::vector<int> &       dnwrd_mididx)
{
    std::vector<ExPolygons5> layers5;
    layers5.resize(layers.size());

    for (auto ll = 0; ll < layers.size(); ll++) {
        // idx0 - bottom of layer, idx1 - top of layer
        int  upwrd_idx0  = ll > 0 ? upwrd_mididx[ll - 1] : -1;
        int  dnwrd_idx0  = ll > 0 ? dnwrd_mididx[ll - 1] : -1;
        int  upwrd_idx1  = upwrd_mididx[ll];
        int  dnwrd_idx1  = dnwrd_mididx[ll];

        auto useMidCut = [](int idx) { return idx != -1; };

        if (!useMidCut(upwrd_idx0) && !useMidCut(dnwrd_idx0) && !useMidCut(upwrd_idx1) && !useMidCut(dnwrd_idx1)) {
            layers5[ll] = ExPolygons5(ExPolygons(layers[ll]));
        } else {
            layers5[ll] = ExPolygons5();
            if (useMidCut(upwrd_idx0) || useMidCut(upwrd_idx1)) {
                layers5[ll].ditherUp[0] = std::move(
                    diff_ex(useMidCut(upwrd_idx0) ? layers_mid[upwrd_idx0] : layers[ll],
                            useMidCut(upwrd_idx1) ? layers_mid[upwrd_idx1] : layers[ll]));
            }
            if (useMidCut(upwrd_idx1)) {
                layers5[ll].ditherUp[1] = std::move(diff_ex(layers[ll], layers_mid[upwrd_idx1]));
            }

            if (useMidCut(dnwrd_idx0) || useMidCut(dnwrd_idx1)) {
                layers5[ll].ditherDn[0] = std::move(
                    diff_ex(useMidCut(dnwrd_idx1) ? layers_mid[dnwrd_idx1] : layers[ll],
                            useMidCut(dnwrd_idx0) ? layers_mid[dnwrd_idx0] : layers[ll]));
            }
            if (useMidCut(dnwrd_idx0)) {
                layers5[ll].ditherDn[1] = std::move(diff_ex(layers[ll], layers_mid[dnwrd_idx0]));
            }

            if (useMidCut(upwrd_idx1) && useMidCut(dnwrd_idx0))
                layers5[ll].whole = std::move(intersection_ex(layers_mid[dnwrd_idx0], layers_mid[upwrd_idx1]));
            else if (useMidCut(upwrd_idx1))
                layers5[ll].whole = std::move(intersection_ex(layers[ll], layers_mid[upwrd_idx1]));
            else if (useMidCut(dnwrd_idx0))
                layers5[ll].whole = std::move(intersection_ex(layers[ll], layers_mid[dnwrd_idx0]));
            else
                layers5[ll].whole = ExPolygons(layers[ll]);
        }
    }
    return layers5;
}
std::vector<ExPolygons5> z_dither(const indexed_triangle_set &   mesh,
                                  const std::vector<float> &     zs,
                                  const MeshSlicingParamsEx &    params,
                                  const std::vector<ExPolygons> &layers,
                                  const std::function<void()> &  throw_on_cancel_callback)
{
    std::vector<float> mid_zs;
    std::vector<int>   upwrd_mididx;
    std::vector<int>   dnwrd_mididx;
    midslice_zs(mesh, zs, params.trafo, params.nozzle_diameter, &mid_zs, &upwrd_mididx, &dnwrd_mididx);
    if (!mid_zs.empty()) {
        std::vector<ExPolygons> layers_mid = slice_mesh_ex(mesh, mid_zs, params, throw_on_cancel_callback);
        return apply_z_dither(layers, layers_mid, upwrd_mididx, dnwrd_mididx);
    } else {
        std::vector<ExPolygons5> layers5;
        for (auto i = 0; i < layers.size(); i++) { layers5.push_back(ExPolygons5((layers[i]))); };
        return layers5;
    }
}

} // namespace Slic3r