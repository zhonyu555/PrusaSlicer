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

#include "LargixHelper.hpp"

#include <Layer.h>
#include <PolygonValidator.h>
#include <PolygonIO.h>
#include <BuildLayer.h>
#include <Size.h>
#include <TeddyDef.h>

namespace Slic3r {

bool LargixHelper::convert_polygon_2_largix(ExPolygon &      src,
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

bool LargixHelper::convert_layer_2_prusa(Largix::Layer &src,
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

bool LargixHelper::convertPolylineToLargix(
    Polyline &                                   pLine1,
    Polyline &                                   pLine2,
    Polyline &                                   pLine3,
    Polyline &                                   pLine4,
    std::vector<std::array<Largix::Point2D, 4>> &pLineOut)
{
    if (pLine1.points.size() != pLine2.points.size() ||
        pLine2.points.size() != pLine3.points.size() ||
        pLine3.points.size() != pLine4.points.size())
        return false;
    for (int i = 0; i < pLine1.points.size(); i++) {
        pLineOut.push_back(std::array<Largix::Point2D, 4>{
            Largix::Point2D(pLine1.points[i].x() * SCALING_FACTOR,
                            pLine1.points[i].y() * SCALING_FACTOR),
            Largix::Point2D(pLine2.points[i].x() * SCALING_FACTOR,
                            pLine2.points[i].y() * SCALING_FACTOR),
            Largix::Point2D(pLine3.points[i].x() * SCALING_FACTOR,
                            pLine3.points[i].y() * SCALING_FACTOR),
            Largix::Point2D(pLine4.points[i].x() * SCALING_FACTOR,
                            pLine4.points[i].y() * SCALING_FACTOR)});
    }
    return true;
}


} // namespace Slic3r
