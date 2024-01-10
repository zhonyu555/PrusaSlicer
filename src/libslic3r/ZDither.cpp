#include "ZDither.hpp"
#include "ExPolygon.hpp"
#include "TriangleMeshSlicer.hpp"
#include "Utils.hpp"


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
                 bool                        dither_both_up_n_down,
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
        // This may be a bit inaccurate if layer heights vary, but will still be an improvement.
        candidate_zs[i] = (zs[i + 1] + zs[i]) / 2;
        layerHeight[i]  = zs[i + 1] - zs[i];
        layerHeightMax  = std::max(layerHeight[i], layerHeightMax);
    }
    layerHeight[n_zs - 1]  = layerHeight[n_zs - 2];
    candidate_zs[n_zs - 1] = zs[n_zs - 1] + layerHeight[n_zs - 1] / 2;

    upwrd_mididx->assign(n_zs, -1);
    dnwrd_mididx->assign(n_zs, -1);

    float nDia = 1.0; // Cooficient to avoid cutting expolys with too vertical triangles (see below)
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

         for (auto cc = start; cc < candidate_zs.size() && candidate_zs[cc] < z_max + eps; cc++) {
            // Ignore facets that are too vertical to fit nDia nozzles at layer height
            if (nozzle_diameter * sqrt(1 - norm[2] * norm[2]) * nDia < fabs(norm[2]) * layerHeight[cc]) {
                if (norm[2] > 0)
                    upwrd_mididx->at(cc) = cc;
                else if (dither_both_up_n_down)
                    dnwrd_mididx->at(cc) = cc;
            }
        }
    }

    if (std::all_of(upwrd_mididx->begin(), upwrd_mididx->end(), [](int idx) { return idx == -1; }) &&
        std::all_of(dnwrd_mididx->begin(), dnwrd_mididx->end(), [](int idx) { return idx == -1; }))
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

namespace {
void export_sublayers_to_svg(size_t            layer_id,
                             const ExPolygons &whole,
                             const ExPolygons &bottom,
                             const ExPolygons &middleUp,
                             const ExPolygons &middleDn,
                             const ExPolygons &top)
{
    BoundingBox bbox = get_extents(whole);
    bbox.merge(get_extents(bottom));
    bbox.merge(get_extents(middleUp));
    bbox.merge(get_extents(middleDn));
    bbox.merge(get_extents(top));
    SVG svg(debug_out_path("z-dither_%d_sublayers.svg", layer_id).c_str(), bbox);
    svg.draw(whole, "green");
    svg.draw(bottom, "lightcoral");
    svg.draw_outline(middleUp, "red", "red", scale_(0.05));
    svg.draw(top, "cyan");
    svg.draw_outline(middleDn, "blue", "blue", scale_(0.05));
    svg.Close();
}

void export_cuts_to_svg(size_t layer_id, const ExPolygons &expoly, const ExPolygons &below, const ExPolygons &above)
{
    BoundingBox bbox = get_extents(expoly);
    bbox.merge(get_extents(below));
    bbox.merge(get_extents(above));
    SVG svg(debug_out_path("z-dither_%d_cuts.svg", layer_id).c_str(), bbox);
    svg.draw_outline(below, "lightcoral", "lightcoral", scale_(0.05));
    svg.draw_outline(expoly, "green", "green", scale_(0.05));
    svg.draw_outline(above, "cyan", "cyan", scale_(0.05));
    svg.Close();
}

// Subtraction of ExPolygons that are very close to each other along some portion of their boundary
// may result in ExPolygons covering tiny, very narrow areas. We need to filter them out.
ExPolygons &filter_tiny_areas(ExPolygons &expolys, double min)
{
    expolys.erase(std::remove_if(expolys.begin(), expolys.end(), [&min](ExPolygon &poly) {
        // Use area and perimeter to estimate width of a closed polygon
        double area   = poly.contour.area();
        double perimeter = poly.contour.length();
        // For cirle area/perimeter = width / 4; For thin rectangle area/perimeter = width / 2. 
        // Arbitrary shape will have average width less than width of a circle 
        double width = area / perimeter * 4;
        return width < scale_(min);
    }), expolys.end());
    return expolys;
}

int total_num_contours(const ExPolygons &expolys)
{
    int total = 0;
    for (const ExPolygon &poly : expolys)
        total += poly.num_contours();
    return total;
}

} // namespace

std::vector<ExPolygons> apply_z_dither(std::vector<ExPolygons> &expolys, 
                                       double min_contour_width,
                                       std::vector<ExPolygons> &expolys_mid,
                                       const std::vector<int> & upwrd_mididx,
                                       const std::vector<int> & dnwrd_mididx,
                                       std::vector<SubLayers> * sublayers)
{
    sublayers->clear();
    sublayers->resize(expolys.size());
    std::vector<ExPolygons> out(expolys.size());

    out[0] = std::move(expolys[0]);     // Do not make sublayers of first layer
    for (auto ll = 1; ll < expolys.size(); ll++) {
        // idx0 - bottom of layer, idx1 - top of layer
        int upwrd_idx0 = upwrd_mididx[ll - 1];
        int dnwrd_idx0 = dnwrd_mididx[ll - 1];
        int upwrd_idx1 = upwrd_mididx[ll];
        int dnwrd_idx1 = dnwrd_mididx[ll];

        auto useMidCut = [](int idx) { return idx != -1; };

        if (useMidCut(dnwrd_idx0) && useMidCut(dnwrd_idx1) &&
            total_num_contours(expolys_mid[dnwrd_idx0]) != total_num_contours(expolys_mid[dnwrd_idx1])) {
            dnwrd_idx0 = dnwrd_idx1 = -1;   // Don't mess up with bridging even in the presence of support 
        }
        if (!useMidCut(upwrd_idx0) && !useMidCut(dnwrd_idx0) && !useMidCut(upwrd_idx1) && !useMidCut(dnwrd_idx1)) {
            out[ll] = std::move(expolys[ll]);
            continue;
        } else {
            ExPolygons bottom, middleUp, middleDn, top, whole;

            if (useMidCut(upwrd_idx0) || useMidCut(upwrd_idx1)) {
                bottom = std::move(
                    diff_ex(useMidCut(upwrd_idx0) ? expolys_mid[upwrd_idx0] : expolys[ll],
                            useMidCut(upwrd_idx1) ? expolys_mid[upwrd_idx1] : expolys[ll]));
            }
            if (useMidCut(upwrd_idx1)) {
                middleUp = std::move(diff_ex(expolys[ll], expolys_mid[upwrd_idx1]));
            }

            if (useMidCut(dnwrd_idx0) || useMidCut(dnwrd_idx1)) {
                top = std::move(
                    diff_ex(useMidCut(dnwrd_idx1) ? expolys_mid[dnwrd_idx1] : expolys[ll],
                            useMidCut(dnwrd_idx0) ? expolys_mid[dnwrd_idx0] : expolys[ll]));
            }
            if (useMidCut(dnwrd_idx0)) {
                middleDn = std::move(diff_ex(expolys[ll], expolys_mid[dnwrd_idx0]));
            }

            filter_tiny_areas(bottom, min_contour_width);
            filter_tiny_areas(middleUp, min_contour_width);
            filter_tiny_areas(middleDn, min_contour_width);
            filter_tiny_areas(top, min_contour_width);
            
            if (!bottom.empty() || !top.empty()) {
                whole = std::move(diff_ex(expolys[ll], top));
                whole = std::move(diff_ex(filter_tiny_areas(whole, min_contour_width), bottom));
                filter_tiny_areas(whole, min_contour_width);
            }
            else {
                out[ll] = std::move(expolys[ll]);
                continue;
            }

            #if 0
            export_sublayers_to_svg(ll, whole, bottom, middleUp, middleDn, top);
            export_cuts_to_svg(ll, expolys[ll], 
                useMidCut(dnwrd_idx0) ? expolys_mid[dnwrd_idx0] : (useMidCut(upwrd_idx0) ? expolys_mid[upwrd_idx0] : ExPolygons()), 
                useMidCut(dnwrd_idx1) ? expolys_mid[dnwrd_idx1] : (useMidCut(upwrd_idx1) ? expolys_mid[upwrd_idx1] : ExPolygons()));
            #endif

            out[ll] = std::move(whole);
            sublayers->at(ll).bottom_ = std::move(bottom);
            sublayers->at(ll).halfUp_ = std::move(middleUp);
            sublayers->at(ll).halfDn_ = std::move(middleDn);
            sublayers->at(ll).top_    = std::move(top);
        }
    }
    return out;
}


std::vector<ExPolygons> z_dither(const indexed_triangle_set &mesh,
                                 const std::vector<float> &zs,
                                 const MeshSlicingParamsEx &params,
                                 std::vector<ExPolygons> &    expolys,
                                 std::vector<SubLayers> *     sublayers,
                                 const std::function<void()> &throw_on_cancel_callback)
{
    std::vector<float> mid_zs;
    std::vector<int>   upwrd_mididx;
    std::vector<int>   dnwrd_mididx;
    midslice_zs(mesh, zs, params.trafo, params.nozzle_diameter, params.z_dither_mode == Z_dither_mode::Both, 
        &mid_zs, &upwrd_mididx, &dnwrd_mididx);
    if (!mid_zs.empty()) {
        std::vector<ExPolygons> expolys_mid = slice_mesh_ex(mesh, mid_zs, params, throw_on_cancel_callback);
        return apply_z_dither(expolys, params.nozzle_diameter / 10, expolys_mid, upwrd_mididx, dnwrd_mididx, sublayers);
    } else {
        *sublayers = std::vector<SubLayers>(expolys.size());
        return expolys;
    }
}

} // namespace Slic3r