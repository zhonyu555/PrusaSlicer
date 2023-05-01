#include "ctb.hpp"
#include "Format/SLAArchiveReader.hpp"
#include "Layer.hpp"

// Special thanks to UVTools for the CTBv4 update and
// Catibo for the excellent writeup(https://github.com/cbiffle/catibo/blob/master/doc/cbddlp-ctb.adoc)

namespace Slic3r {

static void ctb_get_pixel_span(const std::uint8_t* ptr, const std::uint8_t* end,
                               std::uint8_t& pixel, size_t& span_len)
{
    size_t max_len;

    span_len = 0;
    pixel = (*ptr) & 0xF0;
    // the maximum length of the span depends on the pixel color
    max_len = (pixel == 0 || pixel == 0xF0) ? 0xFFF : 0xF;
    while (ptr < end && span_len < max_len && ((*ptr) & 0xF0) == pixel) {
        span_len++;
        ptr++;
    }
}

struct CTBRasterEncoder {
sla::EncodedRaster operator()(const void *ptr, size_t w, size_t h,
                                                size_t num_components)
    {
        std::vector<uint8_t> dst;
        size_t               span_len;
        std::uint8_t         pixel;
        auto                 size = w * h * num_components;
        dst.reserve(size);

        const std::uint8_t *src = reinterpret_cast<const std::uint8_t *>(ptr);
        const std::uint8_t *src_end = src + size;
        while (src < src_end) {
            ctb_get_pixel_span(src, src_end, pixel, span_len);
            src += span_len;
            // fully transparent of fully opaque pixel
            if (pixel == 0 || pixel == 0xF0) {
                pixel = pixel | (span_len >> 8);
                std::copy(&pixel, (&pixel) + 1, std::back_inserter(dst));
                pixel = span_len & 0xFF;
                std::copy(&pixel, (&pixel) + 1, std::back_inserter(dst));
            }
            // antialiased pixel
            else {
                pixel = pixel | span_len;
                std::copy(&pixel, (&pixel) + 1, std::back_inserter(dst));
            }
        }

        return sla::EncodedRaster(std::move(dst), "ctb");
    }
};

namespace {

template <typename T>
T get_cfg_value(const DynamicConfig &cfg,
                const std::string   &key)
{
    T ret;

    if (cfg.has(key)) {
        if (auto opt = cfg.option(key))
            ret = (T)opt->getInt();
    }

    return (T)ret;
}

float_t get_cfg_value(const DynamicConfig &cfg,
                      const std::string   &key)
{
    if (cfg.has(key)) {
        if (auto opt = cfg.option(key))
            return opt->getFloat();
    }

    return 0.0f;
}

void fill_preview(ctb_format_preview &large_preview,
                  ctb_format_preview &small_preview,
                  const ThumbnailsList &thumbnails)
{
    large_preview.size_x = PREV_W; // TODO: FIXME
    large_preview.size_y = PREV_H; // TODO: FIXME
    large_preview.image_len    = sizeof(Slic3r::ctb_preview_data_large);
    large_preview.zero_pad1 = 0;
    large_preview.zero_pad2 = 0;
    large_preview.zero_pad3 = 0;
    large_preview.zero_pad4 = 0;

    small_preview.size_x = PREV_W; // TODO: FIXME
    small_preview.size_y = PREV_H; // TODO: FIXME
    small_preview.image_len    = sizeof(Slic3r::ctb_preview_data_small);
    small_preview.zero_pad1 = 0;
    small_preview.zero_pad2 = 0;
    small_preview.zero_pad3 = 0;
    small_preview.zero_pad4 = 0;

    std::memset(ctb_preview_data_small, 0 , sizeof(ctb_preview_data_small));
    std::memset(ctb_preview_data_large, 0 , sizeof(ctb_preview_data_large));
    if (!thumbnails.empty()) {
        std::uint32_t dst_index;
        std::uint32_t i = 0;
        size_t len;
        size_t pixel_x = 0;
        auto t = thumbnails[0]; //use the first thumbnail
        len = t.pixels.size();
        //sanity check
        if (len != PREV_W * PREV_H * 4)  {
            printf("incorrect thumbnail size. expected %ix%i\n", PREV_W, PREV_H);
            return;
        }
        // rearange pixels: they seem to be stored from bottom to top.
        dst_index = (PREV_W * (PREV_H - 1) * 2);
        while (i < len) {
            std::uint32_t pixel;
            std::uint32_t r = t.pixels[i++];
            std::uint32_t g = t.pixels[i++];
            std::uint32_t b = t.pixels[i++];
            i++; // Alpha
            // convert to BGRA565
            pixel = ((b >> 3) << 11) | ((g >>2) << 5) | (r >> 3);
            ctb_preview_data_large[dst_index++] = pixel & 0xFF;
            ctb_preview_data_large[dst_index++] = (pixel >> 8) & 0xFF;
            pixel_x++;
            if (pixel_x == PREV_W) {
                pixel_x = 0;
                dst_index -= (PREV_W * 4);
            }
        }
    }
}

void fill_header(ctb_format_header          &h,
                 ctb_format_print_params    &print_params,
                 ctb_format_slicer_info     &slicer_info,
                 ctb_format_print_params_v4 &print_params_v4,
                 const SLAPrint             &print,
                 std::uint32_t              layer_count)
{
    CNumericLocalesSetter locales_setter;

    auto        &cfg     = print.full_print_config();

    SLAPrintStatistics              stats = print.print_statistics();

    // v2 MAGIC- 0x12FD0019 (cddlp magic number)
    // v3 MAGIC- 0x12FD0086 (ctb magic number- might be the same for v4)
    // v4 MAGIC- 0x12FD0106 (might be the v3 magic number)
    h.magic                      = 0x12FD0106;
    // Version matches the CTB version
    h.version                    = 4;
    Points bed_shape             = get_cfg_value<Points>(cfg, "bed_shape");
    h.bed_size_x                 = bed_shape[2][0];
    h.bed_size_y                 = bed_shape[2][1];
    h.bed_size_z                 = get_cfg_value<float_t>(cfg, "max_print_height");
    h.zero_pad                   = 0;
    h.layer_height               = get_cfg_value<float_t> (cfg, "layer_height");
    h.overall_height             = layer_count * h.layer_height; // model height- might be a way to get this from prusa slicer
    h.exposure                   = get_cfg_value<float_t> (cfg, "exposure_time");
    h.bot_exposure               = get_cfg_value<float_t> (cfg, "initial_exposure_time");
    h.light_off_delay            = get_cfg_value<float_t> (cfg, "light_off_time");
    h.bot_layer_count            = get_cfg_value<uint32_t> (cfg, "faded_layers");
    h.res_x                      = get_cfg_value<uint32_t> (cfg, "display_pixels_x");
    h.res_y                      = get_cfg_value<uint32_t> (cfg, "display_pixels_y");
    h.large_preview_offset       = sizeof(Slic3r::ctb_format_header);
    // Layer table offset is set below after the v4 params offset is created
    h.layer_count                = layer_count;
    h.small_preview_offset       = h.large_preview_offset + sizeof(Slic3r::ctb_format_preview) + sizeof(Slic3r::ctb_preview_data_large);
    h.print_time                 = stats.estimated_print_time;
    h.projector_type             = 1;  // check for normal or mirrored- 0/1 respectively- LCD printers are "mirrored" for this purpose
    h.print_params_offset        = h.small_preview_offset + sizeof(Slic3r::ctb_format_preview) + sizeof(Slic3r::ctb_preview_data_small);
    h.print_params_size          = sizeof(Slic3r::ctb_format_print_params);
    h.anti_alias_level           = 1;
    h.pwm_level                  = get_cfg_value<uint16_t>(cfg, "light_intensity") * 255 / 100; // TODO: Figure out if these need to be multiplied by 255
    h.bot_pwm_level              = get_cfg_value<uint16_t>(cfg, "bot_light_intensity") * 255 / 100;
    h.encryption_key             = 0;
    h.slicer_info_offset         = h.print_params_offset + h.print_params_size;
    h.slicer_info_size           = sizeof(Slic3r::ctb_format_slicer_info);
    //h.level_set_count            = 0;  // Useless unless antialiasing for cbddlp

    print_params.bot_lift_height      = get_cfg_value<float_t>(cfg, "bot_lift_distance");
    print_params.bot_lift_speed       = get_cfg_value<float_t>(cfg, "bot_lift_speed");
    print_params.lift_height          = get_cfg_value<float_t>(cfg, "lift_distance");
    print_params.lift_speed           = get_cfg_value<float_t>(cfg, "lift_speed");
    print_params.retract_speed        = get_cfg_value<float_t>(cfg, "sla_retract_speed");
    print_params.resin_volume_ml           = get_cfg_value<float_t>(cfg, "bottle_volume");
    print_params.resin_mass_g              = get_cfg_value<float_t>(cfg, "bottle_weight") * 1000.0f;
    print_params.resin_cost                = get_cfg_value<float_t>(cfg, "bottle_cost");
    print_params.bot_light_off_delay      = get_cfg_value<float_t>(cfg, "bot_light_off_time");
    print_params.light_off_delay         = get_cfg_value<float_t>(cfg, "light_off_time");
    print_params.bot_layer_count           = get_cfg_value<uint32_t>(cfg, "faded_layers");
    print_params.zero_pad1                 = 0;
    print_params.zero_pad2                 = 0;
    print_params.zero_pad3                 = 0;
    print_params.zero_pad4                 = 0;

    slicer_info.bot_lift_dist2           = get_cfg_value<float_t>(cfg, "tsmc_bot_lift_height");
    slicer_info.bot_lift_speed2          = get_cfg_value<float_t>(cfg, "tsmc_bot_lift_speed");
    slicer_info.lift_height2             = get_cfg_value<float_t>(cfg, "tsmc_lift_height");
    slicer_info.lift_speed2              = get_cfg_value<float_t>(cfg, "tsmc_lift_speed");
    slicer_info.retract_height2          = get_cfg_value<float_t>(cfg, "tsmc_retract_height");
    slicer_info.retract_speed2           = get_cfg_value<float_t>(cfg, "tsmc_sla_retract_speed");
    slicer_info.rest_time_after_lift     = get_cfg_value<float_t>(cfg, "rest_time_after_lift");
    slicer_info.machine_name_size        = 0;
    slicer_info.anti_alias_flag          = 0; // 0 [No AA] / 8 [AA] for cbddlp files, 7(0x7) [No AA] / 15(0x0F) [AA] for ctb files
    slicer_info.zero_pad1                = 0;
    slicer_info.per_layer_settings       = 0;    // 0 to not support, 0x20 (32) for v3 ctb and 0x40 for v4 ctb files to allow per layer parameters
    slicer_info.timestamp_minutes        = 0;         // Time since epoch in minutes
    slicer_info.antialias_level          = 255;         // Arbitrary for ctb files
    slicer_info.software_version         = 0x01060300;// ctb v3 = 17171200 | ctb v4 pro = 16777216
    slicer_info.rest_time_after_retract  = get_cfg_value<float_t>(cfg, "rest_time_after_retract");
    slicer_info.rest_time_after_lift2    = get_cfg_value<float_t>(cfg, "rest_time_after_lift2");
    slicer_info.transition_layer_count   = get_cfg_value<uint32_t>(cfg, "faded_layers");
    slicer_info.print_params_v4_offset   = h.slicer_info_offset + sizeof(Slic3r::ctb_format_slicer_info);
    slicer_info.zero_pad2                = 0;
    slicer_info.zero_pad3                = 0;
    slicer_info.machine_name          = "RANDOM NAME";

    slicer_info.machine_name_offset      = slicer_info.print_params_v4_offset - sizeof(slicer_info.machine_name);
    h.layer_table_offset         = slicer_info.print_params_v4_offset + sizeof(Slic3r::ctb_format_print_params_v4) + sizeof(Slic3r::ctb_format_print_params_v4_2);

    print_params_v4.bot_retract_speed       = get_cfg_value<float_t>(cfg, "sla_bot_retract_speed");
    print_params_v4.bot_retract_speed2      = get_cfg_value<float_t>(cfg, "tsmc_sla_bot_retract_speed");
    print_params_v4.zero_pad1               = 0;
    print_params_v4.four1                   = 4;
    print_params_v4.zero_pad2               = 0;
    print_params_v4.four2                   = 4;
    print_params_v4.rest_time_after_retract = slicer_info.rest_time_after_retract;
    print_params_v4.rest_time_after_lift    = slicer_info.rest_time_after_lift;
    print_params_v4.rest_time_before_lift   = get_cfg_value<float_t>(cfg, "rest_time_before_lift");
    // Is this the same as bot_lift_dist2?
    print_params_v4.bot_retract_height2     = slicer_info.bot_lift_dist2;
    print_params_v4.unknown1                = 2955.996;
    print_params_v4.unknown2                = 73470;
    print_params_v4.unknown3                = 5;
    print_params_v4.last_layer_index        = get_cfg_value<uint32_t>(cfg, "");
    print_params_v4.zero_pad3               = 0;
    print_params_v4.zero_pad4               = 0;
    print_params_v4.zero_pad5               = 0;
    print_params_v4.zero_pad6               = 0;
    print_params_v4.disclaimer_len          = 320;

    if (layer_count < h.bot_layer_count) {
        h.bot_layer_count = layer_count;
    }
    //material_density = bottle_weight_g / bottle_volume_ml;

    //h.volume_ml = (stats.objects_used_material + stats.support_used_material) / 1000;
    //h.weight_g           = h.volume_ml * material_density;
    //h.price              = (h.volume_ml * bottle_cost) /  bottle_volume_ml;
    //h.price_currency     = '$';

    /*
    h.print_time = (h.bot_layer_count * h.bot_exposure) +
                     ((layer_count - h.bot_layer_count) *
                      h.exposure) +
                     (layer_count * h.lift_dist / h.) +
                     (layer_count * h.lift_height / h.lift_speed) +
                     (layer_count * h.delay_before_exposure_s);
    */
}

} // namespace

std::unique_ptr<sla::RasterBase> CtbSLAArchive::create_raster() const
{
    sla::Resolution     res;
    sla::PixelDim       pxdim;
    std::array<bool, 2> mirror;

    double w  = m_cfg.display_width.getFloat();
    double h  = m_cfg.display_height.getFloat();
    auto   pw = size_t(m_cfg.display_pixels_x.getInt());
    auto   ph = size_t(m_cfg.display_pixels_y.getInt());

    mirror[X] = m_cfg.display_mirror_x.getBool();
    mirror[Y] = m_cfg.display_mirror_y.getBool();

    auto                         ro = m_cfg.display_orientation.getInt();
    sla::RasterBase::Orientation orientation =
        ro == sla::RasterBase::roPortrait ? sla::RasterBase::roPortrait :
                                            sla::RasterBase::roLandscape;

    if (orientation == sla::RasterBase::roPortrait) {
        std::swap(w, h);
        std::swap(pw, ph);
    }

    res   = sla::Resolution{pw, ph};
    pxdim = sla::PixelDim{w / pw, h / ph};
    sla::RasterBase::Trafo tr{orientation, mirror};

    double gamma = m_cfg.gamma_correction.getFloat();

    return sla::create_raster_grayscale_aa(res, pxdim, gamma, tr);
}

sla::RasterEncoder CtbSLAArchive::get_encoder() const
{
    return CTBRasterEncoder{};
}

// Endian safe write of little endian 32bit ints
template <typename T>
static void ctb_write_out(std::ofstream &out, T val)
{
    for(size_t i = 0; i < (sizeof(T) / 8); i++) {
        char i1 = (val & 0xFF);
        out.write((const char *) &i1, 1);
        val = val >> 8;
    }
}

static void ctb_write_out(std::ofstream &out, std::float_t val)
{
    std::uint32_t *f = (std::uint32_t *) &val;
    ctb_write_out(out, *f);
}

static void ctb_write_out(std::ofstream &out, std::string val)
{
    //char *c_str = new char[val.length() + 1];
    //val.c_str();
    out << val;
}

//template <typename T>
static void ctb_write_header(std::ofstream &out, ctb_format_header &h)
{
    boost::pfr::for_each_field(h, [&out](auto &x) {
        ctb_write_out(out, x);
    });
}

static void ctb_write_preview(std::ofstream &out, ctb_format_preview &p)
{
    boost::pfr::for_each_field(p, [&out](auto &x) {
        ctb_write_out(out, x);
    });
}

static void ctb_write_print_params(std::ofstream &out, ctb_format_print_params &h)
{
    boost::pfr::for_each_field(h, [&out](auto &x) {
        ctb_write_out(out, x);
    });
}

static void ctb_write_slicer_info(std::ofstream &out, ctb_format_slicer_info &h)
{
    boost::pfr::for_each_field(h, [&out](auto &x) {
        ctb_write_out(out, x);
    });
}

static void ctb_write_print_params_v4(std::ofstream &out, ctb_format_print_params_v4 &h)
{
    boost::pfr::for_each_field(h, [&out](auto &x) {
        ctb_write_out(out, x);
    });
}

static void ctb_write_print_params_v4_2(std::ofstream &out, ctb_format_print_params_v4_2 &h)
{
    // Garbage data is too big for boost's for_each_field
    for(size_t i = 0; i < std::size(h.reserved); i++) {
        ctb_write_out(out, (uint8_t)0);
    }

    ctb_write_out(out, h.disclaimer_text);
}

static void ctb_write_layer_data(std::ofstream &out, ctb_format_layer_data &h)
{
    boost::pfr::for_each_field(h, [&out](auto &x) {
        ctb_write_out(out, x);
    });
}

static void ctb_write_layer_data_ex(std::ofstream &out, ctb_format_layer_data_ex &h)
{
    boost::pfr::for_each_field(h, [&out](auto &x) {
        ctb_write_out(out, x);
    });
}

void CtbSLAArchive::export_print(const std::string     fname,
                               const SLAPrint       &print,
                               const ThumbnailsList &thumbnails,
                               const std::string    &/*projectname*/)
{
    std::uint32_t layer_count = m_layers.size();

    ctb_format_header          header = {};
    ctb_format_preview         large_preview = {};
    ctb_format_preview         small_preview = {};
    ctb_format_print_params    print_params = {};
    ctb_format_slicer_info     slicer_info = {};
    ctb_format_print_params_v4 print_params_v4 = {};
    ctb_format_print_params_v4_2 print_params_v4_2 = {};
    ctb_format_layer_data      layer_data = {};
    ctb_format_layer_data_ex   layer_data_ex = {};
    std::vector<uint8_t>       layer_images;

    fill_header(header, print_params, slicer_info, print_params_v4, print, layer_count);

    print_params_v4.disclaimer_offset       = layer_data.data_offset - sizeof(print_params_v4_2.disclaimer_text);

    large_preview.image_offset = header.small_preview_offset - sizeof(Slic3r::ctb_preview_data_large);
    small_preview.image_offset = header.print_params_offset - sizeof(Slic3r::ctb_preview_data_small);
    fill_preview(large_preview, small_preview, thumbnails);

    layer_data.pos_z                        = 0.0f;
    layer_data.data_offset                  = header.layer_table_offset;
    layer_data.table_size                   = 36 + sizeof(Slic3r::ctb_format_layer_data_ex);  // 36 add LayerHeaderEx table_size if v4
    layer_data.unknown1                     = 0;
    layer_data.unknown2                     = 0;
    layer_data_ex.rest_time_before_lift   = print_params_v4.rest_time_before_lift;
    layer_data_ex.rest_time_after_lift    = print_params_v4.rest_time_after_lift;
    layer_data_ex.rest_time_after_retract = print_params_v4.rest_time_after_retract;

    try {
        // open the file and write the contents
        std::ofstream out;
        out.open(fname, std::ios::binary | std::ios::out | std::ios::trunc);
        ctb_write_header(out, header);
        ctb_write_preview(out, large_preview);
        ctb_write_preview(out, small_preview);
        ctb_write_print_params(out, print_params);
        ctb_write_slicer_info(out, slicer_info);
        ctb_write_print_params_v4(out, print_params_v4);
        ctb_write_print_params_v4_2(out, print_params_v4_2);

        //layers
        layer_images.reserve(layer_count * LAYER_SIZE_ESTIMATE);
        const uint32_t C_LAYER_DATA_OFFSETS = sizeof(ctb_format_layer_data) * 2 + sizeof(ctb_format_layer_data_ex);
        size_t i = 0;
        for (const sla::EncodedRaster &rst : m_layers) {
            layer_data.page_num = layer_data.data_offset + sizeof(ctb_format_layer_data) * \
                                  layer_count * header.anti_alias_level / PAGE_SIZE;    // I'm not 100% sure if I did this correctly

            ctb_format_layer_data layer_data;
            layer_data.data_offset += C_LAYER_DATA_OFFSETS;
            layer_data.data_size = rst.size();
            layer_data_ex.tot_size = sizeof(layer_data) + sizeof(layer_data_ex) + layer_data.data_size;
            if (i < header.bot_layer_count) {
                layer_data.exposure                   = header         .bot_exposure;
                layer_data.light_off_delay            = print_params   .bot_light_off_delay;
                layer_data_ex.lift_height             = print_params   .bot_lift_height;
                layer_data_ex.lift_speed              = print_params   .bot_lift_speed;
                layer_data_ex.lift_height2            = slicer_info    .bot_lift_dist2;
                layer_data_ex.lift_speed2             = slicer_info    .bot_lift_speed2;
                layer_data_ex.retract_speed           = print_params_v4.bot_retract_speed;
                layer_data_ex.retract_height2         = print_params_v4.bot_retract_height2;
                layer_data_ex.retract_speed2          = print_params_v4.bot_retract_speed2;
                layer_data_ex.light_pwm               = header         .bot_pwm_level;
            } else {
                layer_data.exposure                   = header      .exposure;
                layer_data.light_off_delay            = print_params.light_off_delay;
                layer_data_ex.lift_height             = print_params.lift_height;
                layer_data_ex.lift_speed              = print_params.lift_speed;
                layer_data_ex.lift_height2            = slicer_info .lift_height2;
                layer_data_ex.lift_speed2             = slicer_info .lift_speed2;
                layer_data_ex.retract_speed           = print_params.retract_speed;
                layer_data_ex.retract_height2         = slicer_info .retract_height2;
                layer_data_ex.retract_speed2          = slicer_info .retract_speed2;
                layer_data_ex.light_pwm               = header      .pwm_level;
            }
            ctb_write_layer_data(out, layer_data);
            ctb_write_layer_data(out, layer_data);
            ctb_write_layer_data_ex(out, layer_data_ex);
            // add the rle encoded layer image into the buffer
            out << rst.data();
            layer_data.data_offset += layer_data.data_size;
            layer_data.pos_z       += header.layer_height; // TODO: FIXME
            i++;
        }
        out.close();
    } catch(std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << e.what();
        // Rethrow the exception
        throw;
    }

}

} // namespace Slic3r
