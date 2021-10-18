#ifndef slic3r_LargixHelper_hpp_
#define slic3r_LargixHelper_hpp_

#include <polygon.h>
#include <point.h>
#include <layer.h>
#include <buildlayer.h>

#include "../libslic3r.h"

#include "Polygon.h"

namespace Largix {
class Layer;
}

namespace Slic3r {

class Surface;

class LargixHelper
{
public:

	static bool convert_polygon_2_largix(ExPolygon &src, Largix::Polygon &dst);
    static bool convert_layer_2_prusa(Largix::Layer &src, Polylines &dst);
    static bool convert_layer_2_prusa_1(Largix::Layer &src, Polylines &dst);
    static bool convertPolylineToLargix(
        Polyline &                                   pLine,
        std::vector<Largix::Point2D> &pLineOut);
    static bool convertPolylineToLargix(
        Polyline &                                   pLine1,
        Polyline &                                   pLine2,
        Polyline &                                   pLine3,
        Polyline &                                   pLine4,
        std::vector<std::array<Largix::Point2D, 4>> &pLineOut);
    static void saveLargixStrand(
        std::vector<std::array<Largix::Point2D, 4>> &strand);
};

}; // namespace Slic3r

#endif // slic3r_LargixHelper_hpp_
