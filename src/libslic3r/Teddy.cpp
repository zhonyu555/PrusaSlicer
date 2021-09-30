#include <TeddyConvert.h>
#include <TeddySlice4Convert.h>
#include <Exports.h>
#include <ConvertSettings.h>
#include <vector>
#include <array>
#include <Point.h>



#include "libslic3r.h"
#include "I18N.hpp"
#include "GCode.hpp"
#include "Teddy.hpp"
#include "Exception.hpp"
#include "ExtrusionEntity.hpp"
#include "Print.hpp"
#include "Layer.hpp"


namespace Slic3r {

bool Teddy::do_export(Print *print, const char *path)
{
    Largix::Slices          slices;
    Largix::ConvertSettings settings;
    for (auto object : print->objects()) {
        std::vector<GCode::LayerToPrint> layers_to_print;
        layers_to_print.reserve(object->layers().size() +
                                object->support_layers().size());
        for (auto layer : object->layers()) {
            Largix::Slice4 slice;
            slice.first = layer->slice_z;
            for (auto region : layer->regions()) {
                auto pLines = region->fills.as_polylines();
                std::vector<std::array<Largix::Point2D, 4>> lines;
                lines.reserve(pLines.size() / 4);
                for (int i = 0; i < pLines.size(); i += 4) {
                    convertPolylineToLargix(pLines.at(i), pLines.at(i+1),
                                            pLines.at(i+2), pLines.at(i + 3),
                                            lines);
                }
                slice.second.swap(lines);
            }

            Largix::TeddySlice4Convert conv(slice, settings);
            conv.convert();
            slices.push_back(conv.getSlice());
        }
    }

    Largix::TeddyConvert convert(slices, settings);
    if (!convert.convert()) { return false; }

    if (!writeTeddySCV(path, convert.getProgram())) { return false; }

    return true;
}

bool Teddy::convertPolylineToLargix(
                            Polyline &                              pLine1,
                            Polyline &                              pLine2,
                            Polyline &                              pLine3,
                            Polyline &                              pLine4,
                            std::vector<std::array<Largix::Point2D, 4>> &pLineOut)
{
    if (pLine1.points.size() != pLine2.points.size() ||
        pLine2.points.size() != pLine3.points.size() ||
        pLine3.points.size() != pLine4.points.size())
        return false;
    for (int i = 0; i < pLine1.points.size(); i++) {
        pLineOut.push_back(std::array<Largix::Point2D, 4> {
            Largix::Point2D(pLine1.points[i].x() * SCALING_FACTOR,pLine1.points[i].y() * SCALING_FACTOR),
            Largix::Point2D(pLine2.points[i].x() * SCALING_FACTOR,pLine2.points[i].y() * SCALING_FACTOR),
            Largix::Point2D(pLine3.points[i].x() * SCALING_FACTOR,pLine3.points[i].y() * SCALING_FACTOR),
            Largix::Point2D(pLine4.points[i].x() * SCALING_FACTOR,pLine4.points[i].y() * SCALING_FACTOR)}
        );
    }
    return true;
}
} // namespace Slic3r
