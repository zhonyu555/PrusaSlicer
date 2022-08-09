#include "../Print.hpp"
#include "../ShortestPath.hpp"

#include "FillLightning.hpp"
#include "Lightning/Generator.hpp"

namespace Slic3r::FillLightning {

void Filler::_fill_surface_single(
    const FillParams              &params,
    unsigned int                   thickness_layers,
    const std::pair<float, Point> &direction,
    ExPolygon                      expolygon,
    Polylines                     &polylines_out)
{
    const Layer &layer      = generator->getTreesForLayer(this->layer_id);
    Polylines    fill_lines = layer.convertToLines(to_polygons(expolygon), scaled<coord_t>(0.5 * this->spacing - this->overlap));

    if (params.dont_connect() || fill_lines.size() <= 1) {
        append(polylines_out, chain_polylines(std::move(fill_lines)));
    } else
        connect_infill(std::move(fill_lines), expolygon, polylines_out, this->spacing, params);
}

void GeneratorDeleter::operator()(Generator *p) {
    delete p;
}

GeneratorPtr build_generator(const PrintObject &print_object, const std::function<void()> &throw_on_cancel_callback)
{
    return GeneratorPtr(new Generator(print_object, throw_on_cancel_callback));
}

float Filler::_calibration_density_ratio(size_t index) const
{
    // Calibration ratios for following densities: 1, 5, 10, 20, 40, 60, 80, 99 %
    const std::array<float, 8> density_calibration =
        {1.68f,        1.453846154f, 1.453846154f, 1.453846154f,
         1.467961165f, 1.535025381f, 1.475121951f, 1.584f};
    return density_calibration[std::min(index, density_calibration.size() - 1)];
}

} // namespace Slic3r::FillAdaptive
