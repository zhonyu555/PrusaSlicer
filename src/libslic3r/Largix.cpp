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
#include "Largix.hpp"
#include "Exception.hpp"
#include "ExtrusionEntity.hpp"
#include "Print.hpp"
#include "Layer.hpp"
#include "LargixHelper.hpp"


namespace Slic3r {

bool LargixExport::do_export(Print *print, const char *path)
{
    Largix::Slices          slices;
    Largix::ConvertSettings settings;
    for (auto object : print->objects()) {
        std::vector<GCode::LayerToPrint> layers_to_print;
        layers_to_print.reserve(object->layers().size() +
                                object->support_layers().size());
        for (auto layer : object->layers()) {
            Largix::Slice slice;
            slice.first = layer->slice_z;
            for (auto region : layer->regions()) {
                auto pLines = region->fills.as_polylines();
                std::vector<Largix::Point2D> line_out;
                for (auto line : pLines) 
                {
                    LargixHelper::convertPolylineToLargix(line, line_out);
                }
                slice.second.swap(line_out);
            }

            Largix::TeddySliceConvert conv(slice, settings);
            conv.convert();
            slices.push_back(conv.getSlice());
        }
    }

    Largix::TeddyConvert convert(slices, settings);
    if (!convert.convert()) { return false; }

    if (!writeTeddyCSV(path, convert.getProgram())) { return false; }

    return true;
}

} // namespace Slic3r
