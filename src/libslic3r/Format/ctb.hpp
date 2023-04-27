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

#define PAGE_SIZE 4_294_967_296;

namespace Slic3r {

using ConfMap = std::map<std::string, std::string>;

typedef struct ctb_format_header
{
    std::uint32_t magic;             // 0x12fd_0019 for cbddlp; 0x12fd_0086 for ctb
    std::uint32_t version;           // Version number
    std::float_t  bed_size_x;        // Dimensions of the printer's output in mm
    std::float_t  bed_size_y;        // |
    std::float_t  bed_size_z;        // |
    std::uint64_t zero_pad;
    std::float_t  overall_height; // height of the model in millimeters
    std::float_t  layer_height;   // layer height used at slicing in mm; actual height used by machine in layer table
    std::float_t  exposure;        // exposure time setting used at slicing in seconds for normal layers; actual time in layer table
    std::float_t  bot_exposure;    // exposure time setting used at slicing in seconds for bottom layers; actual time in layer table
    std::float_t  light_off_delay;  //
    std::uint32_t bot_layer_count;   //
    std::uint32_t res_x;                //
    std::uint32_t res_y;                //
    std::uint32_t large_preview_offset;
    std::uint32_t layer_table_offset;
    std::uint32_t layer_count;     // Gets the number of records in the layer table for the first level set. In ctb files, that's equivalent to the total number of records
    std::uint32_t small_preview_offset;  // Gets the file offsets of ImageHeader records describing the smaller preview images.
    std::uint32_t print_time;
    std::uint32_t projector_type;
    std::uint32_t print_params_offset;
    std::uint32_t print_params_size;
    std::uint32_t anti_alias_level;
    std::uint16_t pwm_level;
    std::uint16_t bot_pwm_level;
    std::uint32_t encryption_key;
    std::uint32_t slicer_info_offset;
    std::uint32_t slicer_info_size;
} ctb_format_header;

typedef struct ctb_format_preview
{
    std::uint32_t size_x;
    std::uint32_t size_y;
    std::uint32_t image_offset;
    std::uint32_t image_len;
    std::uint32_t zero_pad1 = 0;
    std::uint32_t zero_pad2 = 0;
    std::uint32_t zero_pad3 = 0;
    std::uint32_t zero_pad4 = 0;
} ctb_format_preview;

// raw image data in BGR565 format FIXME
std::uint8_t ctb_preview_data_large[PREV_W * PREV_H * 2];
std::uint8_t ctb_preview_data_small[PREV_W * PREV_H * 2];

typedef struct ctb_format_print_params
{
    std::float_t  bot_lift_height;         // In mm
    std::float_t  bot_lift_speed;   // In mmpm
    std::float_t  lift_height;
    std::float_t  lift_speed;
    std::float_t  retract_speed;
    std::float_t  resin_volume_ml;
    std::float_t  resin_mass_g;
    std::float_t  resin_cost;
    std::float_t  bot_light_off_delay; // In seconds
    std::float_t  light_off_delay;
    std::uint32_t bot_layer_count;
    std::uint32_t zero_pad1 = 0;
    std::uint32_t zero_pad2 = 0;
    std::uint32_t zero_pad3 = 0;
    std::uint32_t zero_pad4 = 0;
} ctb_format_print_params;

typedef struct ctb_format_slicer_info
{
    std::float_t  bot_lift_dist2;
    std::float_t  bot_lift_speed2;
    std::float_t  lift_height2;
    std::float_t  lift_speed2;
    std::float_t  retract_height2;
    std::float_t  retract_speed2;
    std::float_t  rest_time_after_lift;
    std::uint32_t machine_name_offset;
    std::uint32_t machine_name_size;
    std::uint8_t  anti_alias_flag;       // 0 [No AA] / 8 [AA] for cbddlp files, 7(0x7) [No AA] / 15(0x0F) [AA] for ctb files
    std::uint16_t zero_pad1 = 0;
    std::uint8_t  per_layer_settings = 0;    // 0 to not support, 0x20 (32) for v3 ctb and 0x40 for v4 ctb files to allow per layer parameters
    std::uint32_t timestamp_minutes;         // Time since epoch in minutes
    std::uint32_t antialias_level;
    std::uint32_t software_version;      // ctb v3 = 17171200 | ctb v4 pro = 16777216
    std::float_t  rest_time_after_retract;
    std::float_t  rest_time_after_lift2;
    std::uint32_t transition_layer_count;
    std::uint32_t print_params_v4_offset;
    std::uint32_t zero_pad2 = 0;
    std::uint32_t zero_pad3 = 0;
    std::string   machine_name;
} ctb_format_slicer_info;

// ADDME!
typedef struct ctb_format_print_params_v4
{
    std::float_t  bot_retract_speed;
    std::float_t  bot_retract_speed2;
    std::uint32_t zero_pad1 = 0;
    std::float_t  four1 = 4.0f; // I don't think anyone knows why but ok chitu
    std::uint32_t zero_pad2 = 0;
    std::float_t  four2 = 4.0f; // I don't think anyone knows why but ok chitu
    std::float_t  rest_time_after_retract;
    std::float_t  rest_time_after_lift;
    std::float_t  rest_time_before_lift;
    std::float_t  bot_retract_height2;
    std::float_t  unknown1 = 2955.996;  // 2955.996 or uint:1161347054 but changes
    std::uint32_t unknown2 = 73470;  // 73470 but changes
    std::uint32_t unknown3 = 5;  // 5 apparently??
    std::uint32_t last_layer_index;
    std::uint32_t zero_pad3 = 0;
    std::uint32_t zero_pad4 = 0;
    std::uint32_t zero_pad5 = 0;
    std::uint32_t zero_pad6 = 0;
    std::uint32_t disclaimer_offset;
    std::uint32_t disclaimer_len = 320; // pretty much always going to be 320
} ctb_print_params_v4;

typedef struct ctb_format_print_params_v4_2 {
    std::uint8_t  reserved[384] = {0};
    std::string   disclaimer_text = "Layout and record format for the ctb and cbddlp file types are the copyrighted programs or codes of CBD Technology (China) Inc..The Customer or User shall not in any manner reproduce, distribute, modify, decompile, disassemble, decrypt, extract, reverse engineer, lease, assign, or sublicense the said programs or codes.";
} ctb_format_print_params_v4_2;

typedef struct ctb_format_layer_data
{
    std::float_t  pos_z;
    std::float_t  exposure;        // In seconds
    std::float_t  light_off_delay;
    std::uint32_t data_offset;
    std::uint32_t data_size;
    std::uint32_t page_num;
    std::uint32_t table_size;  // 36 add LayerHeaderEx table_size if v4
    std::uint32_t unknown1 = 0;
    std::uint32_t unknown2 = 0;
} ctb_format_layer_data;

typedef struct ctb_format_layer_data_ex
{
	// Technically in the data, leaving it here for reference
	// Chitu wants this layer_data twice in a row, unsure why
    //ctb_format_layer_data layer_data;
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
} ctb_format_layer_data_ex;

struct CtbRasterEncoder
{
    sla::EncodedRaster operator()(const void *ptr, size_t w, size_t h, size_t num_components);
} CtbRasterEncoder;

class CtbFormatSLAConfigDef : public ConfigDef
{
public:
    CtbFormatSLAConfigDef()
    {
        add(CFG_LIFT_DISTANCE, coFloat);
        add(CFG_LIFT_SPEED, coFloat);
        add(CFG_RETRACT_SPEED, coFloat);
        add(CFG_DELAY_BEFORE_EXPOSURE, coFloat);
        add(CFG_BOTTOM_LIFT_DISTANCE, coFloat);
        add(CFG_BOTTOM_LIFT_SPEED, coFloat);
    }
};

class CtbSLAArchive: public SLAArchiveWriter {
    SLAPrinterConfig m_cfg;
    // TODO: Implement other CTB versions?
    uint16_t m_version;

protected:
    std::unique_ptr<sla::RasterBase> create_raster() const override;
    sla::RasterEncoder get_encoder() const override;

    SLAPrinterConfig & cfg() { return m_cfg; }
    const SLAPrinterConfig & cfg() const { return m_cfg; }

public:

    CtbSLAArchive() = default;
    explicit CtbSLAArchive(const SLAPrinterConfig &cfg):
	m_cfg(cfg), m_version(4) {}
    explicit CtbSLAArchive(SLAPrinterConfig &&cfg):
	m_cfg(std::move(cfg)), m_version(4) {}
    explicit CtbSLAArchive(const SLAPrinterConfig &cfg, uint16_t version):
        m_cfg(cfg), m_version(version) {}
    explicit CtbSLAArchive(SLAPrinterConfig &&cfg, uint16_t version):
        m_cfg(std::move(cfg)), m_version(version) {}



    void export_print(const std::string     fname,
                      const SLAPrint       &print,
                      const ThumbnailsList &thumbnails,
                      const std::string    &projectname = "") override;
};

} // Slic3r

#endif // _SLIC3R_FORMAT_CTB_HPP_
