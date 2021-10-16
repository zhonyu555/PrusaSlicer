#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <PolygonValidator.h>
#include <PolygonHelper.h>
#include <Size.h>
#include <TeddyDef.h>
#include <Settings.h>

#include "../ClipperUtils.hpp"
#include "../ExPolygon.hpp"
#include "../ShortestPath.hpp"
#include "../Surface.hpp"

#include <sstream> 

#include "FillLargix.hpp"
#include "LargixHelper.hpp"

#include <Layer.h>
#include <PolygonValidator.h>
#include <PolygonIO.h>
#include <BuildLayer.h>
#include <Size.h>
#include <TeddyDef.h>

namespace Slic3r {

void FillLargix::_fill_surface_single(
    const FillParams                &params,
    unsigned int                     thickness_layers,
    const std::pair<float, Point>   &direction,
    ExPolygon                        expolygon,
    Polylines                       &polylines_out)
{
    static int  _count = 0;
    Largix::Polygon pol;
    LargixHelper::convert_polygon_2_largix(expolygon, pol);
    if (pol.outer().size() == 0) 
    { 
        assert(!"Empty polygon for filling");
        return; 
    }
    Largix::PolygonValidator pv(pol);
    if (!pv.correct(pol)) {
        assert(!"Failed to correct polygon, it is not valid.");
        return;
    }

    pv.simplify(pol);

    Largix::Settings set;
    set.szBin = Largix::Size2D(Largix::STRAND_4_WIDTH, Largix::STRAND_HEIGHT);
    if (params.printer_options)
    {
        set.maxNumbersStrandsPerLayer = params.printer_options->printer_largix_strands_number;
        set.minStrandRadius = params.printer_options->printer_largix_min_radius;
        set.minStrandLength = params.printer_options->printer_largix_min_strand_lenght;
    }
    else
    {
        set.minStrandLength = set.szBin[1] * 2.5;
        set.minStrandRadius = 5;
        set.maxNumbersStrandsPerLayer = 1;
    }

    Largix::Layer layer;
    Largix::BuildLayer buider(pol, set);

    buider.build(layer);

    if (layer.getNumBins() == 0 ||
        std::any_of(layer.strands().begin(), layer.strands().end(),
                    [](const Largix::Strand &item) { return !item.isClosed(); })) 
    {
        std::stringstream ss;
        ss << "C:\\Temp\\Polygons\\polygon" << (++_count) << ".wkt";
        Largix::PolygonIO::saveToWktFile(pol, ss.str());
    }

    LargixHelper::convert_layer_2_prusa(layer, polylines_out);

}

} // namespace Slic3r
