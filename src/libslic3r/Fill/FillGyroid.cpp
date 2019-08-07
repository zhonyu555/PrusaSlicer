#include "../ClipperUtils.hpp"
#include "../ShortestPath.hpp"
#include "../Surface.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

#include "FillGyroid.hpp"

namespace Slic3r {

static inline double f(double x, double z_sin, double z_cos, bool vertical, bool flip)
{
    if (vertical) {
        double phase_offset = (z_cos < 0 ? M_PI : 0) + M_PI;
        double a   = sin(x + phase_offset);
        double b   = - z_cos;
        double res = z_sin * cos(x + phase_offset + (flip ? M_PI : 0.));
        double r   = sqrt(sqr(a) + sqr(b));
        return asin(a/r) + asin(res/r) + M_PI;
    }
    else {
        double phase_offset = z_sin < 0 ? M_PI : 0.;
        double a   = cos(x + phase_offset);
        double b   = - z_sin;
        double res = z_cos * sin(x + phase_offset + (flip ? 0 : M_PI));
        double r   = sqrt(sqr(a) + sqr(b));
        return (asin(a/r) + asin(res/r) + 0.5 * M_PI);
    }
}

static inline Polyline make_wave(
    const std::vector<Vec2d>& one_period, double width, double height, double offset, double scaleFactor,
    double z_cos, double z_sin, bool vertical, bool flip)
{
    std::vector<Vec2d> points = one_period;
    double period = points.back()(0);
    if (width != period) // do not extend if already truncated
    {
        points.reserve(one_period.size() * floor(width / period));
        points.pop_back();

        int n = points.size();
        do {
            points.emplace_back(Vec2d(points[points.size()-n](0) + period, points[points.size()-n](1)));
        } while (points.back()(0) < width - EPSILON);

        points.emplace_back(Vec2d(width, f(width, z_sin, z_cos, vertical, flip)));
    }

    // and construct the final polyline to return:
    Polyline polyline;
    polyline.points.reserve(points.size());
    for (auto& point : points) {
        point(1) += offset;
        point(1) = clamp(0., height, double(point(1)));
        if (vertical)
            std::swap(point(0), point(1));
        polyline.points.emplace_back((point * scaleFactor).cast<coord_t>());
    }

    return polyline;
}

static std::vector<Vec2d> make_one_period(double width, double scaleFactor, double z_cos, double z_sin, bool vertical, bool flip, double tolerance)
{
    std::vector<Vec2d> points;
    double dx = M_PI_2; // exact coordinates on main inflexion lobes
    double limit = std::min(2*M_PI, width);
    points.reserve(ceil(limit / tolerance / 3));

    for (double x = 0.; x < limit - EPSILON; x += dx) {
        points.emplace_back(Vec2d(x, f(x, z_sin, z_cos, vertical, flip)));
    }
    points.emplace_back(Vec2d(limit, f(limit, z_sin, z_cos, vertical, flip)));

    // piecewise increase in resolution up to requested tolerance
    for(;;)
    {
        size_t size = points.size();
        for (unsigned int i = 1;i < size; ++i) {
            auto& lp = points[i-1]; // left point
            auto& rp = points[i];   // right point
            double x = lp(0) + (rp(0) - lp(0)) / 2;
            double y = f(x, z_sin, z_cos, vertical, flip);
            Vec2d ip = {x, y};
            if (std::abs(cross2(Vec2d(ip - lp), Vec2d(ip - rp))) > sqr(tolerance)) {
                points.emplace_back(std::move(ip));
            }
        }

        if (size == points.size())
            break;
        else
        {
            // insert new points in order
            std::sort(points.begin(), points.end(),
                      [](const Vec2d &lhs, const Vec2d &rhs) { return lhs(0) < rhs(0); });
        }
    }

    return points;
}

static Polylines make_gyroid_waves(double gridZ, double density_adjusted, double line_spacing, double width, double height)
{
    const double scaleFactor = scale_(line_spacing) / density_adjusted;

    // tolerance (in scaled units) - note: clamp the maximum tolerance
    // as there's no benefit to reduce the definition with large nozzles
    const double tolerance = std::min(line_spacing, 0.4) / 2 / unscale<double>(scaleFactor);

 //scale factor for 5% : 8 712 388
 // 1z = 10^-6 mm ?
    const double z     = gridZ / scaleFactor;
    const double z_sin = sin(z);
    const double z_cos = cos(z);

    bool vertical = (std::abs(z_sin) <= std::abs(z_cos));
    double lower_bound = 0.;
    double upper_bound = height;
    bool flip = true;
    if (vertical) {
        flip = false;
        lower_bound = -M_PI;
        upper_bound = width - M_PI_2;
        std::swap(width,height);
    }

    std::vector<Vec2d> one_period_odd = make_one_period(width, scaleFactor, z_cos, z_sin, vertical, flip, tolerance); // creates one period of the waves, so it doesn't have to be recalculated all the time
    flip = !flip;                                                                   // even polylines are a bit shifted
    std::vector<Vec2d> one_period_even = make_one_period(width, scaleFactor, z_cos, z_sin, vertical, flip, tolerance);
    Polylines result;

    for (double y0 = lower_bound; y0 < upper_bound + EPSILON; y0 += M_PI) {
        // creates odd polylines
        result.emplace_back(make_wave(one_period_odd, width, height, y0, scaleFactor, z_cos, z_sin, vertical, flip));
        // creates even polylines
        y0 += M_PI;
        if (y0 < upper_bound + EPSILON) {
            result.emplace_back(make_wave(one_period_even, width, height, y0, scaleFactor, z_cos, z_sin, vertical, flip));
        }
    }

    return result;
}

void FillGyroid::_fill_surface_single(
    const FillParams                &params, 
    unsigned int                     thickness_layers,
    const std::pair<float, Point>   &direction, 
    ExPolygon                       &expolygon, 
    Polylines                       &polylines_out)
{
    float infill_angle = this->angle + (CorrectionAngle * 2*M_PI) / 360.;
    if(abs(infill_angle) >= EPSILON)
        expolygon.rotate(-infill_angle);

    BoundingBox bb = expolygon.contour.bounding_box();
    // Density adjusted to have a good %of weight.
    double      density_adjusted = std::max(0., params.density * 2.44);
    // Distance between the gyroid waves in scaled coordinates.
    coord_t     distance = coord_t(scale_(this->spacing) / density_adjusted);

    // align bounding box to a multiple of our grid module
    bb.merge(_align_to_grid(bb.min, Point(2*M_PI*distance, 2*M_PI*distance)));

    // generate pattern
    Polylines polylines_square = make_gyroid_waves(
        scale_(this->z),
        density_adjusted,
        this->spacing,
        ceil(bb.size()(0) / distance) + 1.,
        ceil(bb.size()(1) / distance) + 1.);

    // clip pattern to boundaries, keeping the polyline order & ordering the fragment to be able to join them easily
    Polylines polylines_chained;
    for (size_t idx_polyline = 0; idx_polyline < polylines_square.size(); ++idx_polyline) {
        // shift the polyline to the grid origin
        Polyline &poly_to_cut = polylines_square[idx_polyline];
        poly_to_cut.translate(bb.min);

        // intersect
        Polylines polylines_to_sort = intersection_pl(Polylines() = { poly_to_cut }, (Polygons)expolygon);
        for (Polyline &polyline : polylines_to_sort) {
            //TODO: replace by closest_index_point()
            if (idx_polyline % 2 == 0) {
                if (poly_to_cut.points.front().distance_to_square(polyline.points.front()) > poly_to_cut.points.front().distance_to_square(polyline.points.back())) {
                    polyline.reverse();
                }
            } else {
                if (poly_to_cut.points.back().distance_to_square(polyline.points.front()) > poly_to_cut.points.back().distance_to_square(polyline.points.back())) {
                    polyline.reverse();
                }
            }
        }
        if (polylines_to_sort.size() > 1) {
            Point nearest = poly_to_cut.points.front();
            if (idx_polyline % 2 != 0) {
                nearest = poly_to_cut.points.back();
            }
            //Bubble sort
            for (size_t idx_sort = polylines_to_sort.size() - 1; idx_sort > 0; idx_sort--) {
                for (size_t idx_bubble = 0; idx_bubble < idx_sort; idx_bubble++) {
                    if (polylines_to_sort[idx_bubble + 1].points.front().distance_to_square(nearest) < polylines_to_sort[idx_bubble].points.front().distance_to_square(nearest)) {
                        iter_swap(polylines_to_sort.begin() + idx_bubble, polylines_to_sort.begin() + idx_bubble + 1);
                    }
                }
            }
        }
        polylines_chained.insert(polylines_chained.end(), polylines_to_sort.begin(), polylines_to_sort.end());
    }

    size_t polylines_out_first_idx = polylines_out.size();
    if (!polylines_chained.empty()) {
        // connect lines
        if (params.dont_connect) {
            polylines_out.insert(polylines_out.end(), polylines_chained.begin(), polylines_chained.end());
        } else {
            this->connect_infill(polylines_chained, expolygon, polylines_out, params);
        }
    }

    //remove too small bits (larger than longer);
    for (size_t idx = polylines_out_first_idx; idx < polylines_out.size(); idx++) {
        if (polylines_out[idx].length() < scale_(this->spacing * 3)) {
            polylines_out.erase(polylines_out.begin() + idx);
            idx--;
        }
    }

    // new paths must be rotated back
    if(abs(infill_angle) >= EPSILON) {
        for (Polylines::iterator it = polylines_out.begin() + polylines_out_first_idx;
             it != polylines_out.end(); ++it) {
            it->rotate(infill_angle);
        }
    }
}

} // namespace Slic3r
