#ifndef SL1_BINARY_HPP
#define SL1_BINARY_HPP

#include "SL1.hpp"

namespace Slic3r {

class SL1_BinaryArchive: public SL1Archive {
protected:

    // Override the factory methods to produce svg instead of a real raster.
    std::unique_ptr<sla::RasterBase> create_raster() const override;
    sla::RasterEncoder get_encoder() const override;

public:

    using SL1Archive::SL1Archive;
};

} // namespace Slic3r

#endif // SL1_BINARY_HPP
