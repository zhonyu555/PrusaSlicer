#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <PolygonValidator.h>
#include <PolygonHelper.h>
#include <PolygonIO.h>
#include <Size.h>
#include <TeddyDef.h>

#include "../ClipperUtils.hpp"
#include "../ExPolygon.hpp"
#include "../ShortestPath.hpp"
#include "../Surface.hpp"

#include <sstream> 


#include "FillLargix.hpp"

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

   
    //std::stringstream  ss;
    //ss << "C:\\Temp\\Polygons\\polygon"  << (++_count) << ".wkt";
    //Largix::PolygonIO::saveToWktFile(pol, ss.str());

    Largix::Layer      layer;

    const Largix::Size2D sz(Largix::STRAND_4_WIDTH, Largix::STRAND_HEIGHT);
    Largix::BuildLayer buider(pol, sz);

    buider.build(layer, 1);

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
            (*it++).push_back(Largix::Point2D(point.x() * SCALING_FACTOR,
                                                point.y() * SCALING_FACTOR));
        }
    }
    
    return true;
}

bool FillLargix::_convert_layer_2_prusa(Largix::Layer &src, Polylines &dst)
{
    
    for (auto strand : src.strands())
    { 
        Polyline pline;
        for (auto bin : strand.bins()) 
        {
            pline.points.push_back(Point::new_scale(bin.center().x(), bin.center().y()));
        }
        dst.push_back(pline);
    }
    
    return true;
}

} // namespace Slic3r
