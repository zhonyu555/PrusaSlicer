#ifndef slic3r_Largix_hpp_
#define slic3r_Largix_hpp_

#include "libslic3r.h"
#include "Print.hpp"

#include <memory>
#include <map>
#include <string>
#include <vector>
#include <Point.h>


namespace Slic3r {    

    class LargixExport
    {
    public:
        LargixExport() {}
        ~LargixExport() = default;

        bool do_export(Print *print, const char *path);
    };
} // namespace Slic3r

#endif
