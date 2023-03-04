#ifndef _SLIC3R_FORMAT_PWMX_HPP_
#define _SLIC3R_FORMAT_PWMX_HPP_

#include <string>

#include "SLAArchiveWriter.hpp"

#include "libslic3r/PrintConfig.hpp"

#define ANYCUBIC_SLA_FORMAT(FILEFORMAT, NAME) \
    { FILEFORMAT, { FILEFORMAT, [] (const auto &cfg) { return std::make_unique<AnycubicSLAArchive>(cfg); } } }

namespace Slic3r {

class AnycubicSLAArchive: public SLAArchiveWriter {
    SLAPrinterConfig m_cfg;

protected:
    std::unique_ptr<sla::RasterBase> create_raster() const override;
    sla::RasterEncoder get_encoder() const override;

    SLAPrinterConfig & cfg() { return m_cfg; }
    const SLAPrinterConfig & cfg() const { return m_cfg; }

public:
    
    AnycubicSLAArchive() = default;
    explicit AnycubicSLAArchive(const SLAPrinterConfig &cfg): m_cfg(cfg) {}
    explicit AnycubicSLAArchive(SLAPrinterConfig &&cfg): m_cfg(std::move(cfg)) {}

    void export_print(const std::string     fname,
                      const SLAPrint       &print,
                      const ThumbnailsList &thumbnails,
                      const std::string    &projectname = "") override;
};


} // namespace Slic3r::sla

#endif // _SLIC3R_FORMAT_PWMX_HPP_
