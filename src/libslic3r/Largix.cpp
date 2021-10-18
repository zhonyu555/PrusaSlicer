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
            Largix::Slice4 slice;
            slice.first = layer->slice_z;
            for (auto region : layer->regions()) {
                auto pLines = region->fills.as_polylines();
                std::vector<std::array<Largix::Point2D, 4>> lines;
                lines.reserve(pLines.size() / 4);
                for (int i = 0; i < pLines.size(); i += 4) {
                    LargixHelper::convertPolylineToLargix(pLines.at(i), pLines.at(i+1),
                                            pLines.at(i+2), pLines.at(i + 3),
                                            lines);
                }
                slice.second.swap(lines);
            }
            if (slice.second.size() > 0) {
                Largix::TeddySlice4Convert conv(slice, settings);
                conv.convert();
                slices.push_back(conv.getSlice());
            } else {
                assert(!"Empty Slice!");
            }
        }
    }

    Largix::TeddyConvert convert(slices, settings);
    if (!convert.convert()) { return false; }

    if (!writeTeddyCSV(path, convert.getProgram())) { return false; }

    return true;
}

} // namespace Slic3r
