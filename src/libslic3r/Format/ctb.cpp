#include "ctb.hpp"

// Special thanks to UVTools for the CTBv4 update and
// Catibo for the excellent writeup(https://github.com/cbiffle/catibo/blob/master/doc/cbddlp-ctb.adoc)

namespace Slic3r {

static void ctb_get_pixel_val(std::uint8_t pixel, uint32_t run_len, std::vector<uint8_t> &dst)
{
    if (run_len == 0) {
        return;
    }

    if (run_len > 1) {
        pixel |= 0x80;
    }
    dst.push_back(pixel);

    if (run_len <= 1) {
        return;
    }

    if (run_len <= 0x7f) {
        dst.push_back((uint8_t) run_len);
        return;
    }

    if (run_len <= 0x3fff) {
        dst.push_back((uint8_t) ((run_len >> 8) | 0x80));
        dst.push_back((uint8_t) run_len);
        return;
    }

    if (run_len <= 0x1fffff) {
        dst.push_back((uint8_t) ((run_len >> 16) | 0xc0));
        dst.push_back((uint8_t) (run_len >> 8));
        dst.push_back((uint8_t) run_len);
        return;
    }

    if (run_len <= 0xfffffff) {
        dst.push_back((uint8_t) ((run_len >> 24) | 0xe0));
        dst.push_back((uint8_t) (run_len >> 16));
        dst.push_back((uint8_t) (run_len >> 8));
        dst.push_back((uint8_t) run_len);
    }
}

struct CTBRasterEncoder
{
    sla::EncodedRaster operator()(const void *ptr, size_t w, size_t h, size_t num_components)
    {
        std::vector<uint8_t> dst;
        uint32_t             run_len = 0;
        std::uint8_t         pixel   = 0xFF;
        auto                 size    = w * h * num_components;
        dst.reserve(size);

        const std::uint8_t *src     = reinterpret_cast<const std::uint8_t *>(ptr);
        const std::uint8_t *src_end = src + size;
        while (src < src_end) {
            uint8_t tmp_pixel = (*src) >> 1;
            if (tmp_pixel == pixel) {
                run_len++;
            } else {
                ctb_get_pixel_val(pixel, run_len, dst);
                pixel   = tmp_pixel;
                run_len = 1;
            }
            src++;
        }
        ctb_get_pixel_val(pixel, run_len, dst);
        dst.shrink_to_fit();

        return sla::EncodedRaster(std::move(dst), "ctb");
    }
};

namespace {

template<typename T> T get_cfg_value(const DynamicConfig &cfg, const std::string &key)
{
    T ret{};

    if (cfg.has(key)) {
        if (auto opt = cfg.option(key)) {
            if (opt->type() == Slic3r::ConfigOptionType::coInt)
                ret = (T) opt->getInt();
            else if (opt->type() == Slic3r::ConfigOptionType::coFloat)
                ret = (T) opt->getFloat();
        }
    }

    return (T) ret;
}

// Need this because of struct padding
// Could use pragma packed or an attribute
// Could also turn this into a compile-time cost with a macro
template<typename T> static size_t get_struct_size(T &h)
{
    size_t tot_size = 0;
    boost::pfr::for_each_field(h, [&tot_size](auto &x) { tot_size += sizeof(x); });
    return tot_size;
}

void fill_preview(ctb_format_preview   &large_preview,
                  ctb_format_preview   &small_preview,
                  ctb_preview_data     &preview_images,
                  const ThumbnailsList &thumbnails)
{
    large_preview.zero_pad1 = 0;
    large_preview.zero_pad2 = 0;
    large_preview.zero_pad3 = 0;
    large_preview.zero_pad4 = 0;

    small_preview.zero_pad1 = 0;
    small_preview.zero_pad2 = 0;
    small_preview.zero_pad3 = 0;
    small_preview.zero_pad4 = 0;

    auto          vec_data    = &preview_images.large;
    std::uint32_t first_width = thumbnails[0].width;
    uint16_t      rep         = 0;
    std::uint32_t prev_pixel{};
    auto          rle = [&]() {
        if (rep == 0) {
            return;
        } else if (rep == 1) {
            vec_data->push_back((prev_pixel & ~RGB565_REPEAT_MASK) & 0xFF);
            vec_data->push_back((prev_pixel >> 8) & 0xFF);
        } else if (rep == 2) {
            for (int i = 0; i < 2; i++) {
                vec_data->push_back((prev_pixel & ~RGB565_REPEAT_MASK) & 0xFF);
                vec_data->push_back((prev_pixel >> 8) & 0xFF);
            }
        } else {
            vec_data->push_back((prev_pixel | RGB565_REPEAT_MASK) & 0xFF);
            vec_data->push_back((prev_pixel >> 8) & 0xFF);
            vec_data->push_back((rep - 1) & 0xFF);
            vec_data->push_back((((rep - 1) | 0x3000) >> 8) & 0xFF);
        }
    };

    for (auto thumb : thumbnails) {
        auto preview = &large_preview;
        if (thumb.width < first_width) {
            vec_data = &preview_images.small;
            preview  = &small_preview;
        } else {
            vec_data = &preview_images.large;
        }

        preview->size_x = thumb.width;
        preview->size_y = thumb.height;

        vec_data->reserve(preview->size_x * preview->size_y * 4);

        // Convert to RGB565 and do RLE Encoding
        std::uint32_t i = thumb.pixels.size() - 1;
        rep             = 0;
        // The color doesn't match the sl1s color- it comes out grey
        // while sl1s comes out orange
        while (i > 3) {
            std::uint16_t pixel;
            i--; // Alpha
            std::uint8_t b = thumb.pixels[i--];
            std::uint8_t g = thumb.pixels[i--];
            std::uint8_t r = thumb.pixels[i--];
            // convert to BGRA565
            pixel = ((r >> 3) << 11) | ((g >> 3) << 6) | (b >> 3);

            if (pixel == prev_pixel) {
                rep++;
                if (rep == RLE_ENCODING_LIMIT) {
                    rle();
                    rep = 0;
                }
            } else {
                rle();
                prev_pixel = pixel;
                rep        = 1;
            }
        }
    }
    large_preview.image_len = preview_images.large.size();
    small_preview.image_len = preview_images.small.size();
}

void fill_header(ctb_format_header          &h,
                 ctb_format_print_params    &print_params,
                 ctb_format_slicer_info     &slicer_info,
                 ctb_format_print_params_v4 &print_params_v4,
                 const SLAPrint             &print,
                 std::uint32_t               layer_count)
{
    CNumericLocalesSetter locales_setter;

    auto &cfg = print.full_print_config();

    SLAPrintStatistics stats = print.print_statistics();

    // v2 MAGIC- 0x12FD0019 (cddlp magic number)
    // v3 MAGIC- 0x12FD0086
    // v4 MAGIC- 0x12FD0106
    h.magic = 0x12FD0106;
    // Version matches the CTB version
    h.version              = 4;
    h.bed_size_x           = get_cfg_value<float>(cfg, "display_width");
    h.bed_size_y           = get_cfg_value<float>(cfg, "display_height");
    h.bed_size_z           = get_cfg_value<float>(cfg, "max_print_height");
    h.zero_pad             = 0;
    h.layer_height         = get_cfg_value<float>(cfg, "layer_height");
    h.overall_height       = layer_count * h.layer_height; // model height- might be a way to get this from prusa slicer
    h.exposure             = get_cfg_value<float>(cfg, "exposure_time");
    h.bot_exposure         = get_cfg_value<float>(cfg, "initial_exposure_time");
    h.light_off_delay      = get_cfg_value<float>(cfg, "light_off_time");
    h.bot_layer_count      = get_cfg_value<uint32_t>(cfg, "faded_layers");
    h.res_x                = get_cfg_value<uint32_t>(cfg, "display_pixels_x");
    h.res_y                = get_cfg_value<uint32_t>(cfg, "display_pixels_y");
    h.large_preview_offset = get_struct_size(h);
    // Layer table offset is set below after the v4 params offset is created
    h.layer_count       = layer_count;
    h.print_time        = stats.estimated_print_time;
    h.projector_type    = 1; // check for normal or mirrored- 0/1 respectively- LCD printers are "mirrored" for this purpose
    h.print_params_size = get_struct_size(print_params);
    h.antialias_level   = 1;
    h.pwm_level         = (uint16_t) (get_cfg_value<float>(cfg, "light_intensity") / 100 * 255);
    h.bot_pwm_level     = (uint16_t) (get_cfg_value<float>(cfg, "bot_light_intensity") / 100 * 255);
    h.encryption_key    = 0;
    h.slicer_info_size  = get_struct_size(slicer_info);
    // h.level_set_count            = 0;  // Useless unless antialiasing for cbddlp

    print_params.bot_lift_height     = get_cfg_value<float>(cfg, "bot_lift_distance");
    print_params.bot_lift_speed      = get_cfg_value<float>(cfg, "bot_lift_speed");
    print_params.lift_height         = get_cfg_value<float>(cfg, "lift_distance");
    print_params.lift_speed          = get_cfg_value<float>(cfg, "lift_speed");
    print_params.retract_speed       = get_cfg_value<float>(cfg, "sla_retract_speed");
    print_params.resin_volume_ml     = get_cfg_value<float>(cfg, "bottle_volume");
    print_params.resin_mass_g        = get_cfg_value<float>(cfg, "bottle_weight") * 1000.0f;
    print_params.resin_cost          = get_cfg_value<float>(cfg, "bottle_cost");
    print_params.bot_light_off_delay = get_cfg_value<float>(cfg, "bot_light_off_time");
    print_params.light_off_delay     = get_cfg_value<float>(cfg, "light_off_time");
    print_params.bot_layer_count     = get_cfg_value<uint32_t>(cfg, "faded_layers");
    print_params.zero_pad1           = 0;
    print_params.zero_pad2           = 0;
    print_params.zero_pad3           = 0;
    print_params.zero_pad4           = 0;

    slicer_info.bot_lift_dist2       = get_cfg_value<float>(cfg, "tsmc_bot_lift_distance");
    slicer_info.bot_lift_speed2      = get_cfg_value<float>(cfg, "tsmc_bot_lift_speed");
    slicer_info.lift_height2         = get_cfg_value<float>(cfg, "tsmc_lift_distance");
    slicer_info.lift_speed2          = get_cfg_value<float>(cfg, "tsmc_lift_speed");
    slicer_info.retract_height2      = get_cfg_value<float>(cfg, "tsmc_retract_height");
    slicer_info.retract_speed2       = get_cfg_value<float>(cfg, "tsmc_sla_retract_speed");
    slicer_info.rest_time_after_lift = get_cfg_value<float>(cfg, "rest_time_after_lift");
    slicer_info.anti_alias_flag      = 0x7; // 0 [No AA] / 8 [AA] for cbddlp files, 7(0x7) [No AA] / 15(0x0F) [AA] for ctb files
    slicer_info.zero_pad1            = 0;
    // TODO: Maybe implement this setting in PrusaSlicer?
    slicer_info.per_layer_settings = 0; // 0 to not support, 0x20 (32) for v3 ctb and 0x40 for v4 ctb files to allow per layer parameters
    slicer_info.timestamp_minutes  = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now().time_since_epoch())
                                        .count(); // Time since epoch in minutes
    // TODO: Fix antialising- make sure it matches other slicers
    slicer_info.antialias_level = 8;
    // TODO: Does this need to be changed?
    slicer_info.software_version        = 0x01090000; // ctb v3 = 17171200 | ctb v4 pro = 16777216
    slicer_info.rest_time_after_retract = get_cfg_value<float>(cfg, "rest_time_after_retract");
    slicer_info.rest_time_after_lift2   = get_cfg_value<float>(cfg, "rest_time_after_lift2");
    slicer_info.transition_layer_count  = get_cfg_value<uint32_t>(cfg, "faded_layers");
    slicer_info.zero_pad2               = 0;
    slicer_info.zero_pad3               = 0;

    print_params_v4.bot_retract_speed       = get_cfg_value<float>(cfg, "sla_bot_retract_speed");
    print_params_v4.bot_retract_speed2      = get_cfg_value<float>(cfg, "tsmc_sla_bot_retract_speed");
    print_params_v4.zero_pad1               = 0;
    print_params_v4.four1                   = 4.0f;
    print_params_v4.zero_pad2               = 0;
    print_params_v4.four2                   = 4.0f;
    print_params_v4.rest_time_after_retract = slicer_info.rest_time_after_retract;
    print_params_v4.rest_time_after_lift    = slicer_info.rest_time_after_lift;
    print_params_v4.rest_time_before_lift   = get_cfg_value<float>(cfg, "rest_time_before_lift");
    print_params_v4.bot_retract_height2     = get_cfg_value<float>(cfg, "tsmc_bot_retract_height");
    print_params_v4.unknown1                = 2955.996;
    print_params_v4.unknown2                = 73470;
    print_params_v4.unknown3                = 5;
    print_params_v4.last_layer_index        = layer_count - 1;
    print_params_v4.zero_pad3               = 0;
    print_params_v4.zero_pad4               = 0;
    print_params_v4.zero_pad5               = 0;
    print_params_v4.zero_pad6               = 0;
    print_params_v4.disclaimer_len          = 320;

    if (layer_count < h.bot_layer_count) {
        h.bot_layer_count = layer_count;
    }
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

    auto                         ro          = m_cfg.display_orientation.getInt();
    sla::RasterBase::Orientation orientation = ro == sla::RasterBase::roPortrait ? sla::RasterBase::roPortrait :
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

sla::RasterEncoder CtbSLAArchive::get_encoder() const { return CTBRasterEncoder{}; }

// Endian safe write of little endian 32bit ints
template<typename T> static void ctb_write_out(std::ofstream &out, T val)
{
    for (size_t i = 0; i < sizeof(T); i++) {
        char i1 = (val & 0xFF);
        out.write((const char *) &i1, 1);
        val = val >> 8;
    }
}

static void ctb_write_out(std::ofstream &out, float val)
{
    std::uint32_t *f = (std::uint32_t *) &val;
    ctb_write_out(out, *f);
}

static void ctb_write_out(std::ofstream &out, std::string val) { out.write(val.c_str(), val.length()); }

template<typename T> static void ctb_write_section(std::ofstream &out, T &h)
{
    boost::pfr::for_each_field(h, [&out](auto &x) { ctb_write_out(out, *&x); });
}

static void ctb_write_preview(std::ofstream &out, ctb_format_preview &large, ctb_format_preview &small, ctb_preview_data &data)
{
    boost::pfr::for_each_field(large, [&out](auto &x) { ctb_write_out(out, x); });

    for (auto i : data.large) {
        ctb_write_out(out, (uint8_t) i);
    }

    boost::pfr::for_each_field(small, [&out](auto &x) { ctb_write_out(out, x); });

    for (auto i : data.small) {
        ctb_write_out(out, (uint8_t) i);
    }
}

static void ctb_write_print_disclaimer(std::ofstream &out, int reserved_size, std::string disclaimer)
{
    // Garbage data is too big for boost's for_each_field
    for (int i = 0; i < reserved_size; i++) {
        ctb_write_out(out, (uint8_t) 0);
    }

    ctb_write_out(out, disclaimer);
}

void CtbSLAArchive::export_print(const std::string     fname,
                                 const SLAPrint       &print,
                                 const ThumbnailsList &thumbnails,
                                 const std::string & /*projectname*/)
{
    std::uint32_t layer_count = m_layers.size();

    ctb_format_header          header{};
    ctb_format_preview         large_preview{};
    ctb_format_preview         small_preview{};
    ctb_preview_data           preview_images{};
    ctb_format_print_params    print_params{};
    ctb_format_slicer_info     slicer_info{};
    ctb_format_print_params_v4 print_params_v4{};
    ctb_format_layer_data      layer_data{};
    ctb_format_layer_data_ex   layer_data_ex{};
    std::vector<uint8_t>       layer_images;

    // This is all zeros- just use the size
    int         reserved_size = 384;
    std::string disclaimer_text =
        "Layout and record format for the ctb and cbddlp file types are the copyrighted programs or codes of CBD Technology (China) "
        "Inc..The Customer or User shall not in any manner reproduce, distribute, modify, decompile, disassemble, decrypt, extract, "
        "reverse engineer, lease, assign, or sublicense the said programs or codes.";

    fill_header(header, print_params, slicer_info, print_params_v4, print, layer_count);
    fill_preview(large_preview, small_preview, preview_images, thumbnails);

    // TODO: Figure out how to get machine name from Prusa
    std::string machine_name      = "ELEGOO";
    slicer_info.machine_name_size = machine_name.length();

    // Fill out all the offsets now that we have the info we need
    header.small_preview_offset        = header.large_preview_offset + get_struct_size(large_preview) + preview_images.large.size();
    header.print_params_offset         = header.small_preview_offset + get_struct_size(small_preview) + preview_images.small.size();
    header.slicer_info_offset          = header.print_params_offset + header.print_params_size;
    slicer_info.machine_name_offset    = header.slicer_info_offset + header.slicer_info_size;
    slicer_info.print_params_v4_offset = slicer_info.machine_name_offset + machine_name.length();
    print_params_v4.disclaimer_offset  = slicer_info.print_params_v4_offset + reserved_size + get_struct_size(print_params_v4);
    header.layer_table_offset          = print_params_v4.disclaimer_offset + disclaimer_text.length();

    large_preview.image_offset = header.small_preview_offset - preview_images.large.size();
    small_preview.image_offset = header.print_params_offset - preview_images.small.size();

    layer_data.pos_z                      = 0.0f;
    layer_data.data_offset                = header.layer_table_offset;
    layer_data.table_size                 = 36 + get_struct_size(layer_data_ex); // 36 add LayerHeaderEx table_size if v4
    layer_data.unknown1                   = 0;
    layer_data.unknown2                   = 0;
    layer_data_ex.rest_time_before_lift   = print_params_v4.rest_time_before_lift;
    layer_data_ex.rest_time_after_lift    = print_params_v4.rest_time_after_lift;
    layer_data_ex.rest_time_after_retract = print_params_v4.rest_time_after_retract;

    try {
        // open the file and write the contents
        std::ofstream out;
        out.open(fname, std::ios::binary | std::ios::out | std::ios::trunc);
        ctb_write_section(out, header);
        ctb_write_preview(out, large_preview, small_preview, preview_images);
        ctb_write_section(out, print_params);
        ctb_write_section(out, slicer_info);
        ctb_write_out(out, machine_name);
        ctb_write_section(out, print_params_v4);
        ctb_write_print_disclaimer(out, reserved_size, disclaimer_text);

        // layers
        layer_images.reserve(layer_count * LAYER_SIZE_ESTIMATE);
        size_t        i                 = 0;
        unsigned long layer_data_offset = header.layer_table_offset + get_struct_size(layer_data) * layer_count;

        for (const sla::EncodedRaster &rst : m_layers) {
            if (i < header.bot_layer_count) {
                layer_data.exposure           = header.bot_exposure;
                layer_data.light_off_delay    = print_params.bot_light_off_delay;
                layer_data_ex.lift_height     = print_params.bot_lift_height;
                layer_data_ex.lift_speed      = print_params.bot_lift_speed;
                layer_data_ex.lift_height2    = slicer_info.bot_lift_dist2;
                layer_data_ex.lift_speed2     = slicer_info.bot_lift_speed2;
                layer_data_ex.retract_speed   = print_params_v4.bot_retract_speed;
                layer_data_ex.retract_height2 = print_params_v4.bot_retract_height2;
                layer_data_ex.retract_speed2  = print_params_v4.bot_retract_speed2;
                layer_data_ex.light_pwm       = header.bot_pwm_level;
            } else {
                layer_data.exposure           = header.exposure;
                layer_data.light_off_delay    = print_params.light_off_delay;
                layer_data_ex.lift_height     = print_params.lift_height;
                layer_data_ex.lift_speed      = print_params.lift_speed;
                layer_data_ex.lift_height2    = slicer_info.lift_height2;
                layer_data_ex.lift_speed2     = slicer_info.lift_speed2;
                layer_data_ex.retract_speed   = print_params.retract_speed;
                layer_data_ex.retract_height2 = slicer_info.retract_height2;
                layer_data_ex.retract_speed2  = slicer_info.retract_speed2;
                layer_data_ex.light_pwm       = header.pwm_level;
            }

            long curr_pos = out.tellp();
            out.seekp(layer_data_offset);

            layer_data_offset += get_struct_size(layer_data) + get_struct_size(layer_data_ex);
            // TODO: This was multiplied by anti_alias_level- find out if i need it
            layer_data.page_num    = layer_data_offset / PAGE_SIZE; // I'm not 100% sure if I did this correctly
            layer_data.data_offset = layer_data_offset - layer_data.page_num * PAGE_SIZE;
            layer_data.data_size   = rst.size();
            layer_data_ex.tot_size = get_struct_size(layer_data) + get_struct_size(layer_data_ex) + layer_data.data_size;

            ctb_write_section(out, layer_data);
            ctb_write_section(out, layer_data_ex);
            out.write(reinterpret_cast<const char *>(rst.data()), rst.size());

            out.seekp(curr_pos);
            ctb_write_section(out, layer_data);

            // add the rle encoded layer image into the buffer
            layer_data_offset += layer_data.data_size;
            layer_data.pos_z += header.layer_height;
            i++;
        }
        out.close();
    } catch (std::exception &e) {
        BOOST_LOG_TRIVIAL(error) << e.what();
        // Rethrow the exception
        throw;
    }
}

} // namespace Slic3r
