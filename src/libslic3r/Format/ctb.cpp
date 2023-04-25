#include "ctb.hpp"

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

sla::EncodedRaster CTBRasterEncoder::operator()(const void *ptr, size_t w, size_t h,
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
            ret = (T)opt;
    }

    return (T)ret;
}

template<class T> void crop_value(T &val, T val_min, T val_max)
{
    if (val < val_min) {
        val = val_min;
    } else if (val > val_max) {
        val = val_max;
    }
}

void fill_preview(ctb_format_preview &p,
                  const ThumbnailsList &thumbnails)
{

    p.size_x    = PREV_W;
    p.size_y    = PREV_H;
    p.data_offset = 10; // FIXME
    p.data_len    = 10; // FIXME

    std::memset(p.pixels, 0 , sizeof(p.pixels));
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
            p.pixels[dst_index++] = pixel & 0xFF;
            p.pixels[dst_index++] = (pixel >> 8) & 0xFF;
            pixel_x++;
            if (pixel_x == PREV_W) {
                pixel_x = 0;
                dst_index -= (PREV_W * 4);
            }
        }
    }
}

void fill_header(ctb_format_header      &h,
                 ctb_format_ext_config  &ext_cfg,
                 ctb_format_ext_config2 &ext_cfg2,
                 const SLAPrint         &print,
                 std::uint32_t          layer_count)
{
    CNumericLocalesSetter locales_setter;

    auto        &cfg     = print.full_print_config();

    SLAPrintStatistics              stats = print.print_statistics();

    // v2 MAGIC- 0x12FD0019
    // v3 MAGIC- 0x12FD0086
    // v4 MAGIC- 0x12FD0106
    h.magic                  = 0x12FD0106;
    // Version matches the CTB version
    h.version                = 4;
    Points bed_shape = get_cfg_value<Points>(cfg, "bed_shape");
    h.printer_out_mm_x           = bed_shape.size.x;
    h.printer_out_mm_y           = bed_shape.size.y;
    h.printer_out_mm_z           = get_cfg_value<uint32_t>(cfg, "max_print_height");
    h.zero_pad                   = 0;
    h.overall_height_mm          = get_cfg_value<float_t> (cfg, ""); // model height // FIXME
    h.layer_height_mm            = get_cfg_value<float_t> (cfg, "layer_height");
    h.exposure_s                 = get_cfg_value<float_t> (cfg, "exposure_time");
    h.bot_exposure_s             = get_cfg_value<float_t> (cfg, "initial_exposure_time");
    h.light_off_time_s           = get_cfg_value<float_t> (cfg, "light_off_time"); // ADDME
    h.bot_layer_count            = get_cfg_value<float_t> (cfg, "faded_layers");
    h.res_x                      = get_cfg_value<float_t> (cfg, "display_pixels_x");
    h.res_y                      = get_cfg_value<float_t> (cfg, "display_pixels_y");
    h.ext_config_offset          = sizeof(Slic3r::ctb_format_header);
    h.ext_config_size            = sizeof(Slic3r::ctb_format_ext_config);
    h.ext_config2_offset         = h.ext_config_offset + h.ext_config_size;
    h.ext_config2_size           = sizeof(Slic3r::ctb_format_ext_config2);
    h.small_preview_offset       = h.ext_config2_offset + h.ext2_config_size;
    h.large_preview_offset       = h.small_preview_offset + sizeof(Slic3r::ctb_format_preview);
    h.layer_table_offset         = h.large_preview_offset + sizeof(Slic3r::ctb_format_preview);
    h.layer_table_count          = m_layers.size(); // FIXME
    h.print_time_s               = stats.estimated_print_time;
    h.projection                 = 1;  // check for normal or mirrored- 0/1 respectively- LCD printers are "mirrored" for this purpose
    h.level_set_count            = 0;  // Useless unless antialiasing for cbddlp
    h.pwm_level                  = get_cfg_value<uint16_t>(cfg, "pwm_level"); // ADDME
    h.bot_pwm_level              = get_cfg_value<uint16_t>(cfg, "bot_pwm_level"); // ADDME
    h.encryption_key             = 0;

    ext_cfg.bot_lift_distance_mm = get_cfg_value<uint32_t>(cfg, "bot_lift_distance"); // ADDME
    ext_cfg.bot_lift_speed_mmpm  = get_cfg_value<uint32_t>(cfg, "bot_lift_speed");    // ADDME
    ext_cfg.lift_distance_mm     = get_cfg_value<uint32_t>(cfg, "lift_distance");     // ADDME
    ext_cfg.lift_speed_mmpm      = get_cfg_value<uint32_t>(cfg, "lift_speed");        // ADDME

    ext_cfg.retract_speed_mmpm   = get_cfg_value<uint32_t>(cfg, "retract_speed");     // ADDME
    ext_cfg.resin_volume_ml      = get_cfg_value<uint32_t>(cfg, "bottle_volume");
    ext_cfg.resin_mass_g         = get_cfg_value<uint32_t>(cfg, "bottle_weight") * 1000.0f;
    ext_cfg.resin_cost           = get_cfg_value<uint32_t>(cfg, "bottle_cost");
    ext_cfg.bot_light_off_time_s = get_cfg_value<uint32_t>(cfg, "bot_light_off_time"); // ADDME
    ext_cfg.light_off_time_s     = get_cfg_value<uint16_t>(cfg, "light_off_time");     // ADDME
    ext_cfg.bot_layer_count      = get_cfg_value<uint16_t>(cfg, "faded_layers");
    ext_cfg.zero_pad1                 = 0;
    ext_cfg.zero_pad2                 = 0;
    ext_cfg.zero_pad3                 = 0;
    ext_cfg.zero_pad4                 = 0;

    ext_cfg2.bot_lift_height2         = ;
	ext_cfg2.bot_lift_speed2          = ;
	ext_cfg2.lift_height2             = ;
	ext_cfg2.lift_speed2              = ;
	ext_cfg2.retract_height2          = ;
	ext_cfg2.retract_speed2           = ;
	ext_cfg2.rest_time_after_lift     = ;
    ext_cfg2.machine_type_offset      = 0;
    ext_cfg2.machine_type_len         = 0;
    ext_cfg2.anti_alias_flag          = 0;
    ext_cfg2.zero_pad1                = 0;
    ext_cfg2.mysterious_id            = 0;
    ext_cfg2.antialias_level          = 255;         // Arbitrary for ctb files
    ext_cfg2.software_version         = 0x01060300;
    ext_cfg2.unknown                  = 0x200;
    ext_cfg2.rest_time_after_retract  =;
    ext_cfg2.rest_time_after_lift2    =;
    ext_cfg2.transition_layer_count   =;
    ext_cfg2.print_parameters_v4_addr =;
    ext_cfg2.zero_pad2                = 0;
    ext_cfg2.zero_pad3                = 0;




    if (layer_count < h.bottom_layer_count) {
        h.bottom_layer_count = layer_count;
    }
    material_density = bottle_weight_g / bottle_volume_ml;

    h.volume_ml = (stats.objects_used_material + stats.support_used_material) / 1000;
    h.weight_g           = h.volume_ml * material_density;
    h.price              = (h.volume_ml * bottle_cost) /  bottle_volume_ml;
    h.price_currency     = '$';
    h.antialiasing       = 1;
    h.per_layer_override = 0;

    // TODO - expose these variables to the UI rather than using material notes
    h.delay_before_exposure_s = get_cfg_value_f(mat_cfg, CFG_DELAY_BEFORE_EXPOSURE, 0.5f);
    crop_value(h.delay_before_exposure_s, 0.0f, 1000.0f);

    h.lift_distance_mm = get_cfg_value_f(mat_cfg, CFG_LIFT_DISTANCE, 8.0f);
    crop_value(h.lift_distance_mm, 0.0f, 100.0f);

    if (mat_cfg.has(CFG_BOTTOM_LIFT_DISTANCE)) {
        m.bottom_lift_distance_mm = get_cfg_value_f(mat_cfg,
                                                    CFG_BOTTOM_LIFT_DISTANCE,
                                                    8.0f);
        crop_value(h.lift_distance_mm, 0.0f, 100.0f);
    } else {
        m.bottom_lift_distance_mm = h.lift_distance_mm;
    }

    h.lift_speed_mms = get_cfg_value_f(mat_cfg, CFG_LIFT_SPEED, 2.0f);
    crop_value(m.bottom_lift_speed_mms, 0.1f, 20.0f);

    if (mat_cfg.has(CFG_BOTTOM_LIFT_SPEED)) {
        m.bottom_lift_speed_mms = get_cfg_value_f(mat_cfg, CFG_BOTTOM_LIFT_SPEED, 2.0f);
        crop_value(m.bottom_lift_speed_mms, 0.1f, 20.0f);
    } else {
        m.bottom_lift_speed_mms = h.lift_speed_mms;
    }

    h.retract_speed_mms = get_cfg_value_f(mat_cfg, CFG_RETRACT_SPEED, 3.0f);
    crop_value(h.lift_speed_mms, 0.1f, 20.0f);

    h.print_time_s = (h.bottom_layer_count * h.bottom_exposure_time_s) +
                     ((layer_count - h.bottom_layer_count) *
                      h.exposure_time_s) +
                     (layer_count * h.lift_distance_mm / h.retract_speed_mms) +
                     (layer_count * h.lift_distance_mm / h.lift_speed_mms) +
                     (layer_count * h.delay_before_exposure_s);


    h.payload_size  = sizeof(h) - sizeof(h.tag) - sizeof(h.payload_size);
    h.pixel_size_um = 50;
}

} // namespace

std::unique_ptr<sla::RasterBase> CtbArchive::create_raster() const
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

sla::RasterEncoder CtbArchive::get_encoder() const
{
    return CTBRasterEncoder{};
}

<Template typename t>
static void ctb_write_header(std::ofstream &out, ctb_format_header &h)
{
    // Non-dirty C++ way
    boost::pfr::for_each_field(t, [](auto &&x)) {
        out.write(x, sizeof(x));
    }

    // Dirty C way
    char *data = reinterpret_cast<char *>(h);
    while(data < sizeof(h)) {
        out.write(data, sizeof(data));
        data++;
    }


    //out.write(TAG_HEADER, sizeof(h.tag));
    /*
    out.write(h.magic,                sizeof(h.magic));
    out.write(h.version,              sizeof(h.version));
    out.write(h.printer_out_mm_x,     sizeof(h.printer_out_mm_x));
    out.write(h.printer_out_mm_y,     sizeof(h.printer_out_mm_y));
    out.write(h.printer_out_mm_z,     sizeof(h.printer_out_mm_z));
    out.write(h.zero_pad,             sizeof(h.zero_pad));
    out.write(h.overall_height_mm,    sizeof(h.overall_height_mm));
    out.write(h.layer_height_mm,      sizeof(h.layer_height_mm));
    out.write(h.exposure_s,           sizeof(h.exposure_s));
    out.write(h.bot_exposure_s,       sizeof(h.bot_exposure_s));
    out.write(h.light_off_time_s,     sizeof(h.light_off_time_s));
    out.write(h.bot_layer_count,      sizeof(h.bot_layer_count));
    out.write(h.res_x,                sizeof(h.res_x));
    out.write(h.res_y,                sizeof(h.res_y));
    out.write(h.large_preview_offset, sizeof(h.large_preview_offset));
    out.write(h.layer_table_offset,   sizeof(h.layer_table_offset));
    out.write(h.layer_table_count,    sizeof(h.layer_table_count));
    out.write(h.small_preview_offset, sizeof(h.small_preview_offset));
    out.write(h.print_time_s,         sizeof(h.print_time_s));
    out.write(h.projection,           sizeof(h.projection));
    out.write(h.ext_config_offset,    sizeof(h.ext_config_offset));
    out.write(h.ext_config_size,      sizeof(h.ext_config_size));
    out.write(h.level_set_count,      sizeof(h.level_set_count));
    out.write(h.pwm_level,            sizeof(h.pwm_level));
    out.write(h.bot_pwm_level,        sizeof(h.bot_pwm_level));
    out.write(h.encryption_key,       sizeof(h.encryption_key));
    out.write(h.ext_config2_offset,   sizeof(h.ext_config2_offset));
    out.write(h.ext_config2_size,     sizeof(h.ext_config2_size));
    */
}

static void ctb_write_ext_config(std::ofstream &out, ctb_format_ext_config &h)
{
    out.write(h.bot_lift_distance_mm, sizeof(h.bot_lift_distance_mm));
    out.write(h.bot_lift_speed_mmpm,  sizeof(h.bot_lift_speed_mmpm));
    out.write(h.lift_distance_mm,     sizeof(h.lift_distance_mm));
    out.write(h.lift_speed_mmpm,      sizeof(h.lift_speed_mmpm));
    out.write(h.retract_speed_mmpm,   sizeof(h.retract_speed_mmpm));
    out.write(h.resin_volume_ml,      sizeof(h.resin_volume_ml));
    out.write(h.resin_mass_g,         sizeof(h.resin_mass_g));
    out.write(h.resin_cost,           sizeof(h.resin_cost));
    out.write(h.bot_light_off_time_s, sizeof(h.bot_light_off_time_s));
    out.write(h.light_off_time_s,     sizeof(h.light_off_time_s));
    out.write(h.bot_layer_count,      sizeof(h.bot_layer_count));
    out.write(h.zero_pad1,            sizeof(h.zero_pad1));
    out.write(h.zero_pad2,            sizeof(h.zero_pad2));
    out.write(h.zero_pad3,            sizeof(h.zero_pad3));
    out.write(h.zero_pad4,            sizeof(h.zero_pad4));

}

static void ctb_write_ext_config2(std::ofstream &out, ctb_format_ext_config2 &h)
{
     out.write(h.bot_lift_height2,             sizeof(h.bot_lift_height2));
     out.write(h.bot_lift_speed2,              sizeof(h.bot_lift_speed2));
     out.write(h.lift_height2,                 sizeof(h.lift_height2));
     out.write(h.lift_speed2,                  sizeof(h.lift_speed2));
     out.write(h.retract_height2,              sizeof(h.retract_height2));
     out.write(h.retract_speed2,               sizeof(h.retract_speed2));
     out.write(h.rest_time_after_lift,         sizeof(h.rest_time_after_lift));
     out.write(h.machine_type_offset,          sizeof(h.machine_type_offset));
     out.write(h.machine_type_len,             sizeof(h.machine_type_len));
     out.write(h.anti_alias_flag,              sizeof(h.anti_alias_flag));
     out.write(h.zero_pad1,                    sizeof(h.zero_pad1));
     out.write(h.mysterious_id,                sizeof(h.mysterious_id));
     out.write(h.antialias_level,              sizeof(h.antialias_level));
     out.write(h.software_version,             sizeof(h.software_version));
     out.write(h.unknown,                      sizeof(h.unknown));
     out.write(h.rest_time_after_retract,      sizeof(h.rest_time_after_retract));
     out.write(h.rest_time_after_lift2,        sizeof(h.rest_time_after_lift2));
     out.write(h.transition_layer_count,       sizeof(h.transition_layer_count));
     out.write(h.print_parameters_v4_addr,     sizeof(h.print_parameters_v4_addr));
     out.write(h.zero_pad2,                    sizeof(h.zero_pad2));
     out.write(h.zero_pad3,                    sizeof(h.zero_pad3));
}

static void ctb_write_preview(std::ofstream &out, ctb_format_preview &p)
{
    //out.write(TAG_PREVIEW, sizeof(p.tag));
    out.write(p.size_x,               sizeof(p.size_x));
    out.write(p.size_y,               sizeof(p.size_y));
    out.write(p.data_offset,          sizeof(p.data_offset));
    out.write(p.data_len,             sizeof(p.data_len));
    out.write(p.pixels,               sizeof(p.pixels));
}

static void ctb_write_layer_header(std::ofstream &out, ctb_format_layers_header &h)
{
    //out.write(TAG_LAYERS, sizeof(h.tag));
    out.write(h.z,                    sizeof(h.z));
    out.write(h.exposure_s,           sizeof(h.exposure_s));
    out.write(h.light_off_time_s,     sizeof(h.light_off_time_s));
    out.write(h.data_offset,          sizeof(h.data_offset));
    out.write(h.data_len,             sizeof(h.data_len));
}

// FIXME
static void ctb_write_layer(std::ofstream &out, ctb_format_layer &l)
{
    out.write(l.image_offset,     sizeof(l.image_offset));
    out.write(l.image_size,       sizeof(l.image_size));
    out.write(l.lift_distance_mm, sizeof(l.lift_distance_mm));
    out.write(l.lift_speed_mms,   sizeof(l.lift_speed_mms));
    out.write(l.exposure_time_s,  sizeof(l.exposure_time_s));
    out.write(l.layer_height_mm,  sizeof(l.layer_height_mm));
    out.write(l.layer44,          sizeof(l.layer44));
    out.write(l.layer48,          sizeof(l.layer48));
}

void CtbArchive::export_print(const std::string     fname,
                               const SLAPrint       &print,
                               const ThumbnailsList &thumbnails,
                               const std::string    &/*projectname*/)
{
    std::uint32_t layer_count = m_layers.size();

    ctb_format_header        header = {};
    ctb_format_ext_config    config = {};
    ctb_format_ext_config2   config2 = {};
    ctb_format_preview       preview = {};
    ctb_format_layer_header  layer_header = {};
    ctb_format_layer_table   layer_table = {};
    std::vector<uint8_t>     layer_images;
    std::uint32_t            image_offset;

    fill_header(header, misc, print, layer_count);
    fill_preview(preview, misc, thumbnails);

    try {
        // open the file and write the contents
        std::ofstream out;
        out.open(fname, std::ios::binary | std::ios::out | std::ios::trunc);
        ctb_write_header(out, header);
        ctb_write_ext_config(out, config);
        ctb_write_ext_config2(out, config2);
        ctb_write_preview(out, preview);
        ctb_write_layer_header(out, layer_header);

        layers_header.payload_size = intro.image_data_offset - intro.layer_data_offset -
                        sizeof(layers_header.tag)  - sizeof(layers_header.payload_size);
        layers_header.layer_count = layer_count;
        ctb_write_layers_header(out, layers_header);

        //layers
        layer_images.reserve(layer_count * LAYER_SIZE_ESTIMATE);
        image_offset = intro.image_data_offset;
        size_t i = 0;
        for (const sla::EncodedRaster &rst : m_layers) {
            ctb_format_layer l;
            std::memset(&l, 0, sizeof(l));
            l.image_offset = image_offset;
            l.image_size = rst.size();
            if (i < header.bottom_layer_count) {
                l.exposure_time_s = header.bottom_exposure_time_s;
                l.layer_height_mm = misc.bottom_layer_height_mm;
                l.lift_distance_mm = misc.bottom_lift_distance_mm;
                l.lift_speed_mms = misc.bottom_lift_speed_mms;
            } else {
                l.exposure_time_s = header.exposure_time_s;
                l.layer_height_mm = header.layer_height_mm;
                l.lift_distance_mm = header.lift_distance_mm;
                l.lift_speed_mms = header.lift_speed_mms;
            }
            image_offset += l.image_size;
            ctb_write_layer(out, l);
            // add the rle encoded layer image into the buffer
            const char* img_start = reinterpret_cast<const char*>(rst.data());
            const char* img_end = img_start + rst.size();
            std::copy(img_start, img_end, std::back_inserter(layer_images));
            i++;
        }
        const char* img_buffer = reinterpret_cast<const char*>(layer_images.data());
        out.write(img_buffer, layer_images.size());
        out.close();
    } catch(std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << e.what();
        // Rethrow the exception
        throw;
    }

}

} // namespace Slic3r

// /////////////////////////////////////////////////////////////////////////////
// Reader implementation
// /////////////////////////////////////////////////////////////////////////////

namespace marchsq {

template<> struct _RasterTraits<Slic3r::png::ImageGreyscale> {
    using Rst = Slic3r::png::ImageGreyscale;

       // The type of pixel cell in the raster
    using ValueType = uint8_t;

       // Value at a given position
    static uint8_t get(const Rst &rst, size_t row, size_t col)
    {
        return rst.get(row, col);
    }

       // Number of rows and cols of the raster
    static size_t rows(const Rst &rst) { return rst.rows; }
    static size_t cols(const Rst &rst) { return rst.cols; }
};

} // namespace marchsq

namespace Slic3r {

template<class Fn> static void foreach_vertex(ExPolygon &poly, Fn &&fn)
{
    for (auto &p : poly.contour.points) fn(p);
    for (auto &h : poly.holes)
        for (auto &p : h.points) fn(p);
}

void invert_raster_trafo(ExPolygons &                  expolys,
                         const sla::RasterBase::Trafo &trafo,
                         coord_t                       width,
                         coord_t                       height)
{
    if (trafo.flipXY) std::swap(height, width);

    for (auto &expoly : expolys) {
        if (trafo.mirror_y)
            foreach_vertex(expoly, [height](Point &p) {p.y() = height - p.y(); });

        if (trafo.mirror_x)
            foreach_vertex(expoly, [width](Point &p) {p.x() = width - p.x(); });

        expoly.translate(-trafo.center_x, -trafo.center_y);

        if (trafo.flipXY)
            foreach_vertex(expoly, [](Point &p) { std::swap(p.x(), p.y()); });

        if ((trafo.mirror_x + trafo.mirror_y + trafo.flipXY) % 2) {
            expoly.contour.reverse();
            for (auto &h : expoly.holes) h.reverse();
        }
    }
}

RasterParams get_raster_params(const DynamicPrintConfig &cfg)
{
    auto *opt_disp_cols = cfg.option<ConfigOptionInt>("display_pixels_x");
    auto *opt_disp_rows = cfg.option<ConfigOptionInt>("display_pixels_y");
    auto *opt_disp_w    = cfg.option<ConfigOptionFloat>("display_width");
    auto *opt_disp_h    = cfg.option<ConfigOptionFloat>("display_height");
    auto *opt_mirror_x  = cfg.option<ConfigOptionBool>("display_mirror_x");
    auto *opt_mirror_y  = cfg.option<ConfigOptionBool>("display_mirror_y");
    auto *opt_orient    = cfg.option<ConfigOptionEnum<SLADisplayOrientation>>("display_orientation");

    if (!opt_disp_cols || !opt_disp_rows || !opt_disp_w || !opt_disp_h ||
        !opt_mirror_x || !opt_mirror_y || !opt_orient)
        throw MissingProfileError("Invalid SL1 / SL1S file");

    RasterParams rstp;

    rstp.px_w = opt_disp_w->value / (opt_disp_cols->value - 1);
    rstp.px_h = opt_disp_h->value / (opt_disp_rows->value - 1);

    rstp.trafo = sla::RasterBase::Trafo{opt_orient->value == sladoLandscape ?
                                            sla::RasterBase::roLandscape :
                                            sla::RasterBase::roPortrait,
                                        {opt_mirror_x->value, opt_mirror_y->value}};

    rstp.height = scaled(opt_disp_h->value);
    rstp.width  = scaled(opt_disp_w->value);

    return rstp;
}

namespace {

ExPolygons rings_to_expolygons(const std::vector<marchsq::Ring> &rings,
                               double px_w, double px_h)
{
    auto polys = reserve_vector<ExPolygon>(rings.size());

    for (const marchsq::Ring &ring : rings) {
        Polygon poly; Points &pts = poly.points;
        pts.reserve(ring.size());

        for (const marchsq::Coord &crd : ring)
            pts.emplace_back(scaled(crd.c * px_w), scaled(crd.r * px_h));

        polys.emplace_back(poly);
    }

    // TODO: Is a union necessary?
    return union_ex(polys);
}

std::vector<ExPolygons> extract_slices_from_sla_archive(
    ZipperArchive           &arch,
    const RasterParams      &rstp,
    const marchsq::Coord    &win,
    std::function<bool(int)> progr)
{
    std::vector<ExPolygons> slices(arch.entries.size());

    struct Status
    {
        double                                 incr, val, prev;
        bool                                   stop  = false;
        execution::SpinningMutex<ExecutionTBB> mutex = {};
    } st{100. / slices.size(), 0., 0.};

    execution::for_each(
        ex_tbb, size_t(0), arch.entries.size(),
        [&arch, &slices, &st, &rstp, &win, progr](size_t i) {
            // Status indication guarded with the spinlock
            {
                std::lock_guard lck(st.mutex);
                if (st.stop) return;

                st.val += st.incr;
                double curr = std::round(st.val);
                if (curr > st.prev) {
                    st.prev = curr;
                    st.stop = !progr(int(curr));
                }
            }

            png::ImageGreyscale img;
            png::ReadBuf        rb{arch.entries[i].buf.data(),
                            arch.entries[i].buf.size()};
            if (!png::decode_png(rb, img)) return;

            constexpr uint8_t isoval = 128;
            auto              rings = marchsq::execute(img, isoval, win);
            ExPolygons        expolys = rings_to_expolygons(rings, rstp.px_w,
                                                            rstp.px_h);

            // Invert the raster transformations indicated in the profile metadata
            invert_raster_trafo(expolys, rstp.trafo, rstp.width, rstp.height);

            slices[i] = std::move(expolys);
        },
        execution::max_concurrency(ex_tbb));

    if (st.stop) slices = {};

    return slices;
}

} // namespace

ConfigSubstitutions CTBReader::read(std::vector<ExPolygons> &slices,
                                    DynamicPrintConfig      &profile_out)
{
    Vec2i windowsize;

    switch(m_quality)
    {
    case SLAImportQuality::Fast: windowsize = {8, 8}; break;
    case SLAImportQuality::Balanced: windowsize = {4, 4}; break;
    default:
    case SLAImportQuality::Accurate:
        windowsize = {2, 2}; break;
    };

    // Ensure minimum window size for marching squares
    windowsize.x() = std::max(2, windowsize.x());
    windowsize.y() = std::max(2, windowsize.y());

    std::vector<std::string> includes = { "ini", "png"};
    std::vector<std::string> excludes = { "thumbnail" };
    ZipperArchive arch = read_zipper_archive(m_fname, includes, excludes);
    auto [profile_use, config_substitutions] = extract_profile(arch, profile_out);

    RasterParams   rstp = get_raster_params(profile_use);
    marchsq::Coord win  = {windowsize.y(), windowsize.x()};
    slices = extract_slices_from_sla_archive(arch, rstp, win, m_progr);

    return std::move(config_substitutions);
}

ConfigSubstitutions CTBReader::read(DynamicPrintConfig &out)
{
    ZipperArchive arch = read_zipper_archive(m_fname, {}, {"png"});
    return out.load(arch.profile, ForwardCompatibilitySubstitutionRule::Enable);
}

} // namespace Slic3r
