///|/ Copyright (c) 2024 Felix Reißmann @felix-rm
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/

#include "GOO.hpp"

#include "Time.hpp"
#include "libslic3r/SLAPrint.hpp"

#include <boost/log/trivial.hpp>
#include <fstream>

namespace Slic3r {

namespace {

// Templated host to network endianess conversion
template<typename T> static T hton(T value) {
    auto *bytes = reinterpret_cast<uint8_t *>(&value);
    for (size_t i = 0; i < sizeof(T) / 2; ++i)
        std::swap(bytes[i], bytes[sizeof(T) - 1 - i]);
    return value;
}

template<typename T> static T constrain(T value, T min, T max) {
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

static float constrained_map(float value, float min, float max, float out_min, float out_max) {
    value = constrain(value, min, max);
    float t = (value - min) / (max - min);
    return out_min * (1 - t) + out_max * t;
}

static void write_thumbnail(
    size_t width, size_t height, const ThumbnailsList &thumbnails, uint16_t *buffer
) {
    if (thumbnails.empty())
        return;

    auto thumbnail = std::find_if(thumbnails.begin(), thumbnails.end(), [&](const ThumbnailData &t) {
        return t.width == width && t.height == height;
    });

    // If no exact match is found choose the largest (last) and let the scaling do the rest
    if (thumbnail == thumbnails.end())
        thumbnail = thumbnails.begin() + (thumbnails.size() - 1);

    auto point_to_index = [](size_t x, size_t y, size_t width, size_t pixel_size) {
        return y * pixel_size * width + x * pixel_size;
    };

    // Scale thumbnail to fit (without cropping)
    size_t thumb_x_max = thumbnail->width - 1;
    size_t thumb_y_max = thumbnail->height - 1;
    size_t thumb_scaling = std::max(thumb_x_max, thumb_y_max);

    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            double x_norm = double(x) / (width - 1) - .5;
            double y_norm = double(y) / (height - 1) - .5;

            int from_x = round(thumb_x_max / 2.0 + x_norm * thumb_scaling);
            int from_y = round(thumb_y_max / 2.0 + y_norm * thumb_scaling);

            if (from_x < 0 || from_x > thumb_x_max || from_y < 0 || from_y > thumb_y_max)
                continue;

            auto to_idx = point_to_index(x, height - 1 - y, width, 1);
            auto from_idx = point_to_index(from_x, from_y, thumbnail->width, 4);

            // Reformat pixel color into uint16_t and write to buffer
            uint16_t r = thumbnail->pixels[from_idx] >> 3;
            uint16_t g = thumbnail->pixels[from_idx + 1] >> 2;
            uint16_t b = thumbnail->pixels[from_idx + 2] >> 3;
            uint16_t pixel = (r << 11) | (g << 5) | b;
            buffer[to_idx] = pixel << 8 | pixel >> 8;
        }
    }
}

template<typename ConfigOptionType>
static const ConfigOption &read_config_value(const DynamicPrintConfig &config, std::string key) {
    static ConfigOptionType fallback{};

    if (config.has(key))
        return *config.option(key);

    return fallback;
}

static void write_config_value(
    char *buffer, size_t size, const DynamicPrintConfig &config, std::string key
) {
    std::string value{};

    if (config.has(key)) {
        auto opt = config.option(key);
        value = opt->serialize();
    }

    strncpy(buffer, value.c_str(), size);
}

static void write_timestamp(char *buffer, size_t size) {
    auto timestamp = Utils::utc_timestamp();
    strncpy(buffer, timestamp.c_str(), size);
}

// NOTE: All members need to be converted to big endian
struct [[gnu::packed]] header_info
{
    char version[4] = {'v', '3', '.', '0'};
    uint8_t magic_tag[8] = {0x07, 0x00, 0x00, 0x00, 0x44, 0x4c, 0x50, 0x00};
    char software_info[32] = SLIC3R_APP_NAME;
    char software_version[24] = SLIC3R_VERSION;
    char file_time[24]{};
    char printer_name[32]{};
    char printer_type[32]{};
    char resin_profile_name[32]{};
    uint16_t aa_level;
    uint16_t grey_level;
    uint16_t blur_level;
    uint16_t small_preview[116 * 116]{};
    uint8_t delimiter_1[2] = {0xd, 0xa};
    uint16_t big_preview[290 * 290]{};
    uint8_t delimiter_2[2] = {0xd, 0xa};
    uint32_t total_layers;
    uint16_t x_resolution;
    uint16_t y_resolution;
    uint8_t x_mirror;
    uint8_t y_mirror;
    float x_platform_size_mm;
    float y_platform_size_mm;
    float z_platform_size_mm;
    float layer_thickness_mm;
    float common_exposure_time_s;
    bool exposure_delivery_time_static;
    float turn_off_time_s;
    float bottom_before_lift_time_s;
    float bottom_after_lift_time_s;
    float bottom_after_retract_time_s;
    float before_lift_time_s;
    float after_lift_time_s;
    float after_retract_time_s;
    float bottom_exposure_time_s;
    uint32_t bottom_layers;
    float bottom_lift_distance_mm;
    float bottom_lift_speed_mm_min;
    float lift_distance_mm;
    float lift_speed_mm_min;
    float bottom_retract_distance_mm;
    float bottom_retract_speed_mm_min;
    float retract_distance_mm;
    float retract_speed_mm_min;
    float bottom_second_lift_distance_mm;
    float bottom_second_lift_speed_mm_min;
    float second_lift_distance_mm;
    float second_lift_speed_mm_min;
    float bottom_second_retract_distance_mm;
    float bottom_second_retract_speed_mm_min;
    float second_retract_distance_mm;
    float second_retract_speed_mm_min;
    uint16_t bottom_light_pwm;
    uint16_t light_pwm;
    bool advance_mode_layer_definition;
    uint32_t printing_time_s;
    float total_volume_mm3;
    float total_weight_g;
    float total_price;
    uint8_t price_unit[8];
    uint32_t layer_content_offset;
    bool grayscale_level = 0;
    uint16_t transition_layers;
};

// NOTE: All members need to be converted to big endian
struct [[gnu::packed]] layer_definition
{
    uint8_t reserved{};
    bool pause_at_layer;
    float pause_lift_distance_mm;
    float position_mm;
    float exposure_time_s;
    float off_time_s;
    float before_lift_time_s;
    float after_lift_time_s;
    float after_retract_time_s;
    float lift_distance_mm;
    float lift_speed_mm_min;
    float second_lift_distance_mm;
    float second_lift_speed_mm_min;
    float retract_distance_mm;
    float retract_speed_mm_min;
    float second_retract_distance_mm;
    float second_retract_speed_mm_min;
    uint16_t light_pwm;
    uint8_t delimiter[2] = {0xd, 0xa};
};

} // namespace

std::unique_ptr<sla::RasterBase> GOOWriter::create_raster() const {
    auto orientation = m_cfg.display_orientation.getInt() == sla::RasterBase::roPortrait ?
        sla::RasterBase::roPortrait :
        sla::RasterBase::roLandscape;

    sla::Resolution resolution{
        static_cast<size_t>(m_cfg.display_pixels_x.getInt()),
        static_cast<size_t>(m_cfg.display_pixels_y.getInt())
    };

    sla::PixelDim pixel_dimensions{
        m_cfg.display_width.getFloat() / resolution.width_px,
        m_cfg.display_height.getFloat() / resolution.height_px
    };

    if (orientation == sla::RasterBase::roPortrait) {
        std::swap(resolution.width_px, resolution.height_px);
        std::swap(pixel_dimensions.w_mm, pixel_dimensions.h_mm);
    }

    std::array<bool, 2> mirror = {};
    mirror[X] = m_cfg.display_mirror_x.getBool();
    mirror[Y] = m_cfg.display_mirror_y.getBool();

    sla::RasterBase::Trafo tr{orientation, mirror};

    double gamma = m_cfg.gamma_correction.getFloat();

    return sla::create_raster_grayscale_aa(resolution, pixel_dimensions, gamma, tr);
}

sla::RasterEncoder GOOWriter::get_encoder() const { return sla::GOORLERasterEncoder{}; }

void GOOWriter::export_print(
    const std::string filename,
    const SLAPrint &print,
    const ThumbnailsList &thumbnails,
    const std::string &project_name
) {
    static constexpr std::array<uint8_t, 11> end_string = {0x00, 0x00, 0x00, 0x07, 0x00, 0x00,
                                                           0x00, 0x44, 0x4c, 0x50, 0x00};

    auto &config = print.full_print_config();
    auto &stats = print.print_statistics();
    auto &obj_stats = print.default_object_config();
    auto &mat = print.material_config();

    auto option_as_float = [](auto const &option) -> float { return option.getFloat(); };
    auto option_as_uint16 = [](auto const &option) -> uint16_t { return option.getInt(); };

    auto orientation = static_cast<sla::RasterBase::Orientation>(m_cfg.display_orientation.getInt());

    sla::Resolution resolution{
        static_cast<size_t>(m_cfg.display_pixels_x.getInt()),
        static_cast<size_t>(m_cfg.display_pixels_y.getInt())
    };

    if (orientation == sla::RasterBase::roPortrait)
        std::swap(resolution.width_px, resolution.height_px);

    // NOTE: We populate exposure, lift, retract and wait parameters here but only the
    //       ones in the layer_definition will be used for printing.
    //       (advance_mode_layer_definition = true)
    //       This allows more control over exposure and layer hight...

    // clang-format off
    header_info h_info{
        .aa_level = hton<uint16_t>(m_cfg.gamma_correction.getFloat() * 100),
        .grey_level = 0, // NOTE: Unset
        .blur_level = 0, // NOTE: Unset
        .total_layers = hton<uint32_t>(stats.slow_layers_count + stats.fast_layers_count),
        .x_resolution = hton<uint16_t>(resolution.width_px),
        .y_resolution = hton<uint16_t>(resolution.height_px),
        .x_mirror = m_cfg.display_mirror_x.getBool(),
        .y_mirror = m_cfg.display_mirror_y.getBool(),
        .x_platform_size_mm = hton(option_as_float(m_cfg.display_width)),
        .y_platform_size_mm = hton(option_as_float(m_cfg.display_height)),
        .z_platform_size_mm = hton(option_as_float(m_cfg.max_print_height)),
        .layer_thickness_mm = hton(option_as_float(obj_stats.layer_height)),
        .common_exposure_time_s = hton(option_as_float(mat.exposure_time)),
        .exposure_delivery_time_static = true,
        .turn_off_time_s = 0, // NOTE: Unused because of exposure_delivery_time_static==true
        .bottom_before_lift_time_s = hton(option_as_float(mat.sla_initial_wait_before_lift)),
        .bottom_after_lift_time_s = hton(option_as_float(mat.sla_initial_wait_after_lift)),
        .bottom_after_retract_time_s = hton(option_as_float(mat.sla_initial_wait_after_retract)),
        .before_lift_time_s = hton(option_as_float(mat.sla_wait_before_lift)),
        .after_lift_time_s = hton(option_as_float(mat.sla_wait_after_lift)),
        .after_retract_time_s = hton(option_as_float(mat.sla_wait_after_retract)),
        .bottom_exposure_time_s = hton(option_as_float(mat.exposure_time)),
        .bottom_layers = hton<uint32_t>(obj_stats.faded_layers + 1), // NOTE: Faded layers + initial layer have increased exposure
        .bottom_lift_distance_mm = hton(option_as_float(mat.sla_initial_primary_lift_distance)),
        .bottom_lift_speed_mm_min = hton(option_as_float(mat.sla_initial_primary_lift_speed)),
        .lift_distance_mm = hton(option_as_float(mat.sla_primary_lift_distance)),
        .lift_speed_mm_min = hton(option_as_float(mat.sla_primary_lift_speed)),
        .bottom_retract_distance_mm = hton(option_as_float(mat.sla_initial_primary_retract_distance)),
        .bottom_retract_speed_mm_min = hton(option_as_float(mat.sla_initial_primary_retract_speed)),
        .retract_distance_mm = hton(option_as_float(mat.sla_primary_retract_distance)),
        .retract_speed_mm_min = hton(option_as_float(mat.sla_primary_retract_speed)),
        .bottom_second_lift_distance_mm = hton(option_as_float(mat.sla_initial_secondary_lift_distance)),
        .bottom_second_lift_speed_mm_min = hton(option_as_float(mat.sla_initial_secondary_lift_speed)),
        .second_lift_distance_mm = hton(option_as_float(mat.sla_secondary_lift_distance)),
        .second_lift_speed_mm_min = hton(option_as_float(mat.sla_secondary_lift_speed)),
        .bottom_second_retract_distance_mm = hton(option_as_float(mat.sla_initial_secondary_retract_distance)),
        .bottom_second_retract_speed_mm_min = hton(option_as_float(mat.sla_initial_secondary_retract_speed)),
        .second_retract_distance_mm = hton(option_as_float(mat.sla_secondary_retract_distance)),
        .second_retract_speed_mm_min = hton(option_as_float(mat.sla_secondary_retract_speed)),
        .bottom_light_pwm = hton(option_as_uint16(mat.initial_exposure_pwm)),
        .light_pwm = hton(option_as_uint16(mat.exposure_pwm)),
        .advance_mode_layer_definition = true, 
        .printing_time_s = hton<uint32_t>(stats.estimated_print_time),
        .total_volume_mm3 = hton<float>(stats.total_weight / mat.material_density.getFloat()),
        .total_weight_g = hton<float>(stats.total_weight),
        .total_price = hton<float>(mat.bottle_cost.getFloat() /
            (mat.bottle_volume.getFloat() * mat.material_density.getFloat()) * stats.total_weight),
        .price_unit = "",
        .layer_content_offset = hton<uint32_t>(sizeof(header_info)),
        .grayscale_level = true,
        .transition_layers = hton(option_as_uint16(obj_stats.faded_layers))
    };
    // clang-format on

    write_timestamp(h_info.file_time, sizeof(h_info.file_time));
    write_thumbnail(116, 116, thumbnails, h_info.small_preview);
    write_thumbnail(290, 290, thumbnails, h_info.big_preview);
    write_config_value(h_info.printer_name, sizeof(h_info.printer_name), config, "printer_model");
    write_config_value(h_info.printer_type, sizeof(h_info.printer_type), config, "printer_variant");
    write_config_value(
        h_info.resin_profile_name, sizeof(h_info.resin_profile_name), config,
        "sla_material_settings_id"
    );

    std::ofstream output{filename, std::ios_base::binary};
    output.write(reinterpret_cast<const char *>(&h_info), sizeof(header_info));

    // Cache value for use in the loop
    auto layer_height = obj_stats.layer_height.getFloat();

    size_t current_layer = 1;
    for (const sla::EncodedRaster &raster : m_layers) {
        bool bottom = current_layer == 1;

        // clang-format off
        layer_definition l_def{
            .pause_at_layer = false,
            .pause_lift_distance_mm = 0,
            .position_mm = hton<float>(
                mat.initial_layer_height.getFloat() + layer_height * (current_layer - 1)
            ),
            .exposure_time_s = hton(constrained_map(
                current_layer, 1, obj_stats.faded_layers + 1, //
                mat.initial_exposure_time, mat.exposure_time
            )),
            .off_time_s = 0,
            .before_lift_time_s = hton(option_as_float( //
                bottom ? mat.sla_initial_wait_before_lift : mat.sla_wait_before_lift
            )),
            .after_lift_time_s = hton(option_as_float( //
                bottom ? mat.sla_initial_wait_after_lift : mat.sla_wait_after_lift
            )),
            .after_retract_time_s = hton(option_as_float( //
                bottom ? mat.sla_initial_wait_after_retract : mat.sla_wait_after_retract
            )),
            .lift_distance_mm = hton(option_as_float( //
                bottom ? mat.sla_initial_primary_lift_distance : mat.sla_primary_lift_distance
            )),
            .lift_speed_mm_min = hton(option_as_float( //
                bottom ? mat.sla_initial_primary_lift_speed : mat.sla_primary_lift_speed
            )),
            .second_lift_distance_mm = hton(option_as_float( //
                bottom ? mat.sla_initial_secondary_lift_distance : mat.sla_secondary_lift_distance
            )),
            .second_lift_speed_mm_min = hton(option_as_float( //
                bottom ? mat.sla_initial_secondary_lift_speed : mat.sla_secondary_lift_speed
            )),
            .retract_distance_mm = hton(option_as_float( //
                bottom ? mat.sla_initial_primary_retract_distance : mat.sla_primary_retract_distance
            )),
            .retract_speed_mm_min = hton(option_as_float( //
                bottom ? mat.sla_initial_primary_retract_speed : mat.sla_primary_retract_speed
            )),
            .second_retract_distance_mm = hton(option_as_float( //
                bottom ? mat.sla_initial_secondary_retract_distance : mat.sla_secondary_retract_distance
            )),
            .second_retract_speed_mm_min = hton(option_as_float( //
                bottom ? mat.sla_initial_secondary_retract_speed : mat.sla_secondary_retract_speed
            )),
            .light_pwm = hton(option_as_uint16(bottom ? mat.initial_exposure_pwm : mat.exposure_pwm))
        };
        // clang-format on

        output.write(reinterpret_cast<const char *>(&l_def), sizeof(layer_definition));
        output.write(reinterpret_cast<const char *>(raster.data()), raster.size());
        current_layer++;
    }

    output.write(reinterpret_cast<const char *>(end_string.data()), end_string.size());
}

ConfigSubstitutions GOOReader::read(
    std::vector<ExPolygons> &slices, DynamicPrintConfig &profile_out
) {
    BOOST_LOG_TRIVIAL(error) << "GOOReader::read not yet implemented";
    return {};
}

} // namespace Slic3r