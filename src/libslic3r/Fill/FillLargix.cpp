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
    this->_convert_polygon_2_largix(expolygon, pol);

    Largix::PolygonValidator pv(pol);
    pv.simplify(pol);
    pv.correct(pol);

    Largix::Layer      layer;

    const Largix::Size2D sz(Largix::STRAND_4_WIDTH, Largix::STRAND_HEIGHT);

    Largix::Settings set;
    set.szBin = Largix::szBin4_;
    set.minStrandLength = set.szBin[1] * 2.5;

    Largix::BuildLayer buider(pol, set);

    buider.build(layer, 1);

    if (std::any_of(layer.strands().begin(), layer.strands().end(),
                    [](const Largix::Strand &item) { return !item.isClosed(); })) 
    {
        std::stringstream ss;
        ss << "C:\\Temp\\Polygons\\polygon" << (++_count) << ".wkt";
        Largix::PolygonIO::saveToWktFile(pol, ss.str());
    }

    this->_convert_layer_2_prusa(layer, polylines_out);

}

bool FillLargix::_convert_polygon_2_largix(ExPolygon       &src,
                                           Largix::Polygon &dst)
{
    for (Slic3r::Point point : src.contour) {
        dst.outer().push_back(Largix::Point2D(point.x() * SCALING_FACTOR,
                                                point.y() * SCALING_FACTOR));
    }
    dst.inners().resize(src.holes.size());
    auto it = dst.inners().begin();
    for (auto poly : src.holes) {
        for (auto point : poly) {
            (*it).push_back(Largix::Point2D(point.x() * SCALING_FACTOR,
                                                point.y() * SCALING_FACTOR));
        }
        it++;
    }
    
    return true;
}

bool FillLargix::_convert_layer_2_prusa(Largix::Layer &  src,
                                        Polylines &      dst)
{
    
    for (auto strand : src.strands())
    { 
        std::vector<std::array<Largix::Point2D, 4>> points;
        strand.get4StrandPoints(points);

        std::array<Polyline,4> pline;
        for (auto point : points) 
        { 
            pline[0].points.push_back(
                Point::new_scale(point[0].x(), point[0].y()));
            pline[1].points.push_back(
                Point::new_scale(point[1].x(), point[1].y()));
            pline[2].points.push_back(
                Point::new_scale(point[2].x(), point[2].y()));
            pline[3].points.push_back(
                Point::new_scale(point[3].x(), point[3].y()));
        }
        dst.push_back(pline[0]);
        dst.push_back(pline[1]);
        dst.push_back(pline[2]);
        dst.push_back(pline[3]);
    }
    
    return true;
}

} // namespace Slic3r
