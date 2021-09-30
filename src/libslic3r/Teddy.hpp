#ifndef slic3r_Teddy_hpp_
#define slic3r_Teddy_hpp_

#include "libslic3r.h"
#include "Print.hpp"

#include <memory>
#include <map>
#include <string>
#include <vector>
#include <Point.h>


namespace Slic3r {    

    class Teddy
    {
    public:
        Teddy() {}
        ~Teddy() = default;

        bool do_export(Print *print, const char *path);
        bool convertPolylineToLargix(Polyline &               pLine1,
                                    Polyline &                    pLine2,
                                    Polyline &                    pLine3,
                                    Polyline &                    pLine4,
                                    std::vector<std::array<Largix::Point2D,4>> &pLineOut);
    };
} // namespace Slic3r

#endif
