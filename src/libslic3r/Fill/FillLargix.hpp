#ifndef slic3r_FillLargix_hpp_
#define slic3r_FillLargix_hpp_

#include <polygon.h>
#include <point.h>
#include <layer.h>
#include <buildlayer.h>

#include "../libslic3r.h"

#include "FillBase.hpp"

#include "Polygon.h"

namespace Largix {
class Layer;
}

namespace Slic3r {

class Surface;

class FillLargix : public Fill
{
public:
    Fill *clone() const override { return new FillLargix(*this); };
    ~FillLargix() override = default;

protected:
	void _fill_surface_single(
	    const FillParams                &params, 
	    unsigned int                     thickness_layers,
	    const std::pair<float, Point>   &direction, 
	    ExPolygon    		             expolygon,
	    Polylines                       &polylines_out) override;

	coord_t _min_spacing;
	coord_t _line_spacing;
	// distance threshold for allowing the horizontal infill lines to be connected into a continuous path
	coord_t _diagonal_distance;
	// only for line infill
	coord_t _line_oscillation;

	Line _line(int i, coord_t x, coord_t y_min, coord_t y_max) const {
		coord_t osc = (i & 1) ? this->_line_oscillation : 0;
		return Line(Point(x - osc, y_min), Point(x + osc, y_max));
	}

	bool _can_connect(coord_t dist_X, coord_t dist_Y)
	{
	    const auto TOLERANCE = coord_t(10 * SCALED_EPSILON);
    	return (dist_X >= (this->_line_spacing - this->_line_oscillation) - TOLERANCE)
        	&& (dist_X <= (this->_line_spacing + this->_line_oscillation) + TOLERANCE)
        	&& (dist_Y <= this->_diagonal_distance);
    }

	bool _convert_polygon_2_largix(ExPolygon &src, Largix::Polygon &dst);
    bool _convert_layer_2_prusa(Largix::Layer & src,
                                Polylines &     dst);
};

}; // namespace Slic3r

#endif // slic3r_FillLargix_hpp_
