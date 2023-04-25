#ifndef _SLIC3R_FORMAT_CTB_HPP_
#define _SLIC3R_FORMAT_CTB_HPP_

#include "GCode/ThumbnailData.hpp"
#include "SLA/RasterBase.hpp"
#include "libslic3r/SLAPrint.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Zipper.hpp"
#include "SLAArchiveWriter.hpp"
#include "SLAArchiveReader.hpp"

#include <sstream>
#include <iostream>
#include <fstream>
#include <string>

#include <boost/algorithm/string/replace.hpp>
#include <boost/log/trivial.hpp>
#include <boost/pfr/core.hpp>

#define TAG_INTRO "ANYCUBIC\0\0\0\0"
#define TAG_HEADER "HEADER\0\0\0\0\0\0"
#define TAG_PREVIEW "PREVIEW\0\0\0\0\0"
#define TAG_LAYERS "LAYERDEF\0\0\0\0"

#define CFG_LIFT_DISTANCE "LIFT_DISTANCE"
#define CFG_LIFT_SPEED "LIFT_SPEED"
#define CFG_RETRACT_SPEED "RETRACT_SPEED"
#define CFG_DELAY_BEFORE_EXPOSURE "DELAY_BEFORE_EXPOSURE"
#define CFG_BOTTOM_LIFT_SPEED "BOTTOM_LIFT_SPEED"
#define CFG_BOTTOM_LIFT_DISTANCE "BOTTOM_LIFT_DISTANCE"

#define PREV_W 224
#define PREV_H 168
#define PREV_DPI 42

#define LAYER_SIZE_ESTIMATE (32 * 1024)

namespace Slic3r {

using ConfMap = std::map<std::string, std::string>;

typedef struct ctb_format_header
{
    std::uint32_t magic;             // 0x12fd_0019 for cbddlp; 0x12fd_0086 for ctb
    std::uint32_t version;           // Always 2 for some reason
    std::uint32_t printer_out_mm_x;  // Dimensions of the printer's output
    std::uint32_t printer_out_mm_y;  // volume in millimeters
    std::uint32_t printer_out_mm_z;
    std::uint64_t zero_pad;
    std::float_t  overall_height_mm; // height of the model in millimeters
    std::float_t  layer_height_mm;   // layer height used at slicing in mm; actual height used by machine in layer table
    std::float_t  exposure_s;        // exposure time setting used at slicing in seconds for normal layers; actual time in layer table
    std::float_t  bot_exposure_s;    // exposure time setting used at slicing in seconds for bottom layers; actual time in layer table
    std::float_t  light_off_time_s;  //
    std::float_t  bot_layer_count;   //
    std::float_t  res_x;                //
    std::float_t  res_y;                //
    std::uint32_t large_preview_offset;
    std::uint32_t layer_table_offset;
    std::uint32_t layer_table_count;     // Gets the number of records in the layer table for the first level set. In ctb files, that's equivalent to the total number of records
    std::uint32_t small_preview_offset;  // Gets the file offsets of ImageHeader records describing the smaller preview images.
    std::uint32_t print_time_s;
    std::uint32_t projection;
    std::uint32_t ext_config_offset;
    std::uint32_t ext_config_size;
    std::uint32_t level_set_count;
    std::uint16_t pwm_level;
    std::uint16_t bot_pwm_level;
    std::uint32_t encryption_key;
    std::uint32_t ext_config2_offset;
    std::uint32_t ext_config2_size;
} ctb_format_header;

typedef struct ctb_format_ext_config
{
    std::float_t  bot_lift_distance_mm;
    std::float_t  bot_lift_speed_mmpm;
    std::float_t  lift_distance_mm;
    std::float_t  lift_speed_mmpm;
    std::float_t  retract_speed_mmpm;
    std::float_t  resin_volume_ml;
    std::float_t  resin_mass_g;
    std::float_t  resin_cost;
    std::float_t bot_light_off_time_s;
    std::float_t light_off_time_s;
    std::uint32_t bot_layer_count;
	// NEW VALUES FIXME
	std::uint32_t zero_pad1;
	std::uint32_t zero_pad2;
	std::uint32_t zero_pad3;
	std::uint32_t zero_pad4;
} ctb_format_ext_config;

typedef struct ctb_format_ext_config2
{
	// NEW VALUES FIXME
	std::float_t  bot_lift_height2;
	std::float_t  bot_lift_speed2;
	std::float_t  lift_height2;
	std::float_t  lift_speed2;
	std::float_t  retract_height2;
	std::float_t  retract_speed2;
	std::float_t  rest_time_after_lift;
    std::uint32_t machine_type_offset;
    std::uint32_t machine_type_len;
	std::uint32_t anti_alias_flag;
    std::uint32_t zero_pad1;
    std::uint32_t mysterious_id;
    std::uint32_t antialias_level;
    std::uint32_t software_version;
	std::float_t  rest_time_after_retract;
	std::float_t  rest_time_after_lift2;
	std::float_t  transition_layer_count;
	std::float_t  print_parameters_v4_addr;
	std::uint32_t zero_pad2;
	std::uint32_t zero_pad3;
} ctb_format_ext_config2;

// ADDME!
typedef struct ctb_print_parameters_v4
{
	std::float_t  bot_retract_speed;
	std::float_t  bot_retract_speed2;
	std::uint32_t zero_pad1;
	std::float_t  four1;
	std::uint32_t zero_pad2;
	std::float_t  four2;
	std::float_t  rest_time_after_retract;
	std::float_t  rest_time_after_lift;
	std::float_t  rest_time_before_lift;
	std::float_t  bot_retract_height2;
	std::float_t  unknown1;
	std::float_t  unknown2;
	std::float_t  unknown3;
	std::uint32_t last_layer_index;
	std::uint32_t zero_pad2;
	std::uint32_t zero_pad2;
	std::uint32_t zero_pad2;
	std::uint32_t zero_pad2;
	std::uint32_t disclaimer_addr;
	std::uint32_t disclaimer_len;
	byte          reserved[384];
}

typedef struct ctb_format_preview
{
    std::uint32_t size_x;
    std::uint32_t size_y;
    std::uint32_t data_offset;
    std::uint32_t data_len;
    // raw image data in BGR565 format FIXME
    std::uint16_t pixels[PREV_W * PREV_H * 2];
	// NEW VALUES FIXME
	std::uint32_t zero_pad1;
	std::uint32_t zero_pad2;
	std::uint32_t zero_pad3;
	std::uint32_t zero_pad4;
} ctb_format_preview;

typedef struct ctb_format_layer_header
{
    std::float_t  z;
    std::float_t exposure_s;
    std::float_t light_off_time_s;
    std::uint32_t data_offset;
    std::uint32_t data_len;
	std::uint32_t page_num;
	// CHANGED VALUES FIXME
	std::uint32_t table_size;  // 36 add LayerHeaderEx table_size if v4
	std::uint32_t zero_pad1;
	std::uint32_t zero_pad2;
} ctb_format_layer_header;

// IM NEW ADDME!
typedef struct ctb_format_layer_header_ex
{
	// I'm not sure if these need to be repeated
    std::float_t  z;
    std::float_t exposure_s;
    std::float_t light_off_time_s;
    std::uint32_t data_offset;
    std::uint32_t data_len;
	std::uint32_t page_num;
	std::uint32_t table_size;  // 36 add LayerHeaderEx table_size if v4
	std::uint32_t zero_pad1;
	std::uint32_t zero_pad2;

	std::uint32_t tot_size;
	std::float_t  lift_height;
	std::float_t  lift_speed;
	std::float_t  lift_height2;
	std::float_t  lift_speed2;
	std::float_t  retract_speed;
	std::float_t  retract_height2;
	std::float_t  retract_speed2;
	std::float_t  rest_time_before_lift;
	std::float_t  rest_time_after_lift;
	std::float_t  rest_time_after_retract;
	std::float_t  light_pwm;
} ctb_format_layer_header_ex;

// FIXME
typedef struct ctb_format_layer_table
{
    std::uint32_t image_offset;
    std::uint32_t image_size;
    std::float_t  lift_distance_mm;
    std::float_t  lift_speed_mms;
    std::float_t  exposure_time_s;
    std::float_t  layer_height_mm;
    std::float_t  layer44; // unkown - usually 0
    std::float_t  layer48; // unkown - usually 0
} ctb_format_layer;

struct CTBRasterEncoder
{
    sla::EncodedRaster operator()(const void *ptr, size_t w size_t h, size_t num_components);
}

class CtbArchive: public SLAArchiveWriter {
    SLAPrinterConfig m_cfg;

protected:
    std::unique_ptr<sla::RasterBase> create_raster() const override;
    sla::RasterEncoder get_encoder() const override;

    SLAPrinterConfig & cfg() { return m_cfg; }
    const SLAPrinterConfig & cfg() const { return m_cfg; }

public:

    CtbArchive() = default;
    explicit CtbArchive(const SLAPrinterConfig &cfg): m_cfg(cfg) {}
    explicit CtbArchive(SLAPrinterConfig &&cfg): m_cfg(std::move(cfg)) {}

    void export_print(const std::string     fname,
                      const SLAPrint       &print,
                      const ThumbnailsList &thumbnails,
                      const std::string    &projectname = "") override;
};

class CtbFormatConfigDef : public ConfigDef
{
public:
    CtbFormatConfigDef()
    {
        add(CFG_LIFT_DISTANCE, coFloat);
        add(CFG_LIFT_SPEED, coFloat);
        add(CFG_RETRACT_SPEED, coFloat);
        add(CFG_DELAY_BEFORE_EXPOSURE, coFloat);
        add(CFG_BOTTOM_LIFT_DISTANCE, coFloat);
        add(CFG_BOTTOM_LIFT_SPEED, coFloat);
    }
};

} // namespace Slic3r::sla

class CTBArchive: public SLAArchiveWriter {
    SLAPrinterConfig m_cfg;

protected:
    std::unique_ptr<sla::RasterBase> create_raster() const override;
    sla::RasterEncoder get_encoder() const override;

    SLAPrinterConfig & cfg() { return m_cfg; }
    const SLAPrinterConfig & cfg() const { return m_cfg; }

    void export_print(Zipper &,
                      const SLAPrint       &print,
                      const ThumbnailsList &thumbnails,
                      const std::string    &projectname);

public:

    CTBArchive() = default;
    explicit CTBArchive(const SLAPrinterConfig &cfg): m_cfg(cfg) {}
    explicit CTBArchive(SLAPrinterConfig &&cfg): m_cfg(std::move(cfg)) {}

    void export_print(const std::string     fname,
                      const SLAPrint       &print,
                      const ThumbnailsList &thumbnails,
                      const std::string    &projectname = "") override;
};

class CTBReader: public SLAArchiveReader {
    SLAImportQuality m_quality = SLAImportQuality::Balanced;
    std::function<bool(int)> m_progr;
    std::string m_fname;

public:
    // If the profile is missing from the archive (older PS versions did not have
    // it), profile_out's initial value will be used as fallback. profile_out will be empty on
    // function return if the archive did not contain any profile.
    ConfigSubstitutions read(std::vector<ExPolygons> &slices,
                             DynamicPrintConfig      &profile_out) override;

    ConfigSubstitutions read(DynamicPrintConfig &profile) override;

    SL1Reader() = default;
    SL1Reader(const std::string       &fname,
              SLAImportQuality         quality,
              std::function<bool(int)> progr)
        : m_quality(quality), m_progr(progr), m_fname(fname)
    {}
};

struct RasterParams {
    sla::RasterBase::Trafo trafo; // Raster transformations
    coord_t        width, height; // scaled raster dimensions (not resolution)
    double         px_h, px_w;    // pixel dimesions
};

RasterParams get_raster_params(const DynamicPrintConfig &cfg);

void invert_raster_trafo(ExPolygons &                  expolys,
                         const sla::RasterBase::Trafo &trafo,
                         coord_t                       width,
                         coord_t                       height);

} // namespace Slic3r::sla

#endif // _SLIC3R_FORMAT_CTB_HPP_
