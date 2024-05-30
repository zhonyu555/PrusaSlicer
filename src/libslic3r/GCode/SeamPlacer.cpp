///|/ Copyright (c) Prusa Research 2020 - 2023 Vojtěch Bubník @bubnikv, Lukáš Matěna @lukasmatena,
/// Pavel Mikuš @Godrak
///|/ Copyright (c) SuperSlicer 2023 Remi Durand @supermerill
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/

#include <boost/filesystem/operations.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "SeamPlacer.hpp"

#include "libslic3r/GCode/SeamAligned.hpp"
#include "libslic3r/GCode/SeamRear.hpp"
#include "libslic3r/GCode/SeamRandom.hpp"
#include "libslic3r/GCode/ModelVisibility.hpp"
#include "libslic3r/GCode/SeamGeometry.hpp"

namespace Slic3r::Seams {

using ObjectShells = std::vector<std::pair<const PrintObject *, Shells::Shells<>>>;
using ObjectPainting = std::map<const PrintObject*, ModelInfo::Painting>;

ObjectShells partition_to_shells(
    SpanOfConstPtrs<PrintObject> objects,
    const Params &params,
    const ObjectPainting& object_painting,
    const std::function<void(void)> &throw_if_canceled
) {
    ObjectShells result;

    for (const PrintObject *print_object : objects) {
        const ModelInfo::Painting &painting{object_painting.at(print_object)};
        throw_if_canceled();

        const std::vector<Geometry::Extrusions> extrusions{
            Geometry::get_extrusions(print_object->layers())};
        const Perimeters::LayerInfos layer_infos{Perimeters::get_layer_infos(
            print_object->layers(), params.perimeter.elephant_foot_compensation
        )};
        Shells::Shells<Polygon> shell_polygons{
            Shells::create_shells(extrusions, params.max_distance)};

        Shells::Shells<> perimeters{
            Perimeters::create_perimeters(shell_polygons, layer_infos, painting, params.perimeter)};
        throw_if_canceled();
        result.emplace_back(print_object, std::move(perimeters));
    }
    return result;
}

ObjectSeams precalculate_seams(
    const Params &params,
    ObjectShells &&seam_data,
    const std::function<void(void)> &throw_if_canceled
) {
    ObjectSeams result;

    for (auto &[print_object, shells] : seam_data) {
        switch (params.seam_preference) {
        case spAligned: {
            const Transform3d transformation{print_object->trafo_centered()};
            const ModelVolumePtrs &volumes{print_object->model_object()->volumes};

            Slic3r::ModelInfo::Visibility
                points_visibility{transformation, volumes, params.visibility, throw_if_canceled};
            throw_if_canceled();
            const Aligned::VisibilityCalculator visibility_calculator{
                points_visibility, params.convex_visibility_modifier,
                params.concave_visibility_modifier};

            result[print_object] = Aligned::get_object_seams(
                std::move(shells), visibility_calculator, params.aligned
            );
            break;
        }
        case spRear: {
            result[print_object] = Rear::get_object_seams(std::move(shells), params.rear_project_threshold);
            break;
        }
        case spRandom: {
            result[print_object] = Random::get_object_seams(std::move(shells), params.random_seed);
            break;
        }
        case spNearest: {
            throw std::runtime_error("Cannot precalculate seams for nearest position!");
        }
        }
        throw_if_canceled();
    }
    return result;
}

Params Placer::get_params(const DynamicPrintConfig &config) {
    Params params{};

    params.perimeter.elephant_foot_compensation = config.opt_float("elefant_foot_compensation");
    if (config.opt_int("raft_layers") > 0) {
        params.perimeter.elephant_foot_compensation = 0.0;
    }
    params.random_seed = 1653710332u;

    params.aligned.max_detour = 1.0;
    params.aligned.continuity_modifier = 2.0;
    params.convex_visibility_modifier = 1.1;
    params.concave_visibility_modifier = 0.9;
    params.perimeter.overhang_threshold = Slic3r::Geometry::deg2rad(55.0);
    params.perimeter.convex_threshold = Slic3r::Geometry::deg2rad(10.0);
    params.perimeter.concave_threshold = Slic3r::Geometry::deg2rad(15.0);

    params.seam_preference = config.opt_enum<SeamPosition>("seam_position");
    params.staggered_inner_seams = config.opt_bool("staggered_inner_seams");

    params.max_nearest_detour = 1.0;
    params.rear_project_threshold = 0.05; // %
    params.aligned.jump_visibility_threshold = 0.6;
    params.max_distance = 5.0;
    params.perimeter.oversampling_max_distance = 0.2;
    params.perimeter.embedding_threshold = 0.5;
    params.perimeter.painting_radius = 0.1;
    params.perimeter.simplification_epsilon = 0.001;
    params.perimeter.smooth_angle_arm_length = 0.2;
    params.perimeter.sharp_angle_arm_length = 0.05;

    params.visibility.raycasting_visibility_samples_count = 30000;
    params.visibility.fast_decimation_triangle_count_target = 16000;
    params.visibility.sqr_rays_per_sample_point = 5;

    return params;
}

ObjectLayerPerimeters sort_to_layers(ObjectShells &&object_shells) {
    ObjectLayerPerimeters result;
    for (auto &[print_object, shells] : object_shells) {
        const std::size_t layer_count{print_object->layer_count()};
        result[print_object] = LayerPerimeters(layer_count);

        for (Shells::Shell<> &shell : shells) {
            for (Shells::Slice<> &slice : shell) {
                const BoundingBox bounding_box{Geometry::scaled(slice.boundary.positions)};
                result[print_object][slice.layer_index].push_back(
                    BoundedPerimeter{std::move(slice.boundary), bounding_box}
                );
            }
        }
    }
    return result;
}

void Placer::init(
    SpanOfConstPtrs<PrintObject> objects,
    const Params &params,
    const std::function<void(void)> &throw_if_canceled
) {
    BOOST_LOG_TRIVIAL(debug) << "SeamPlacer: init: start";

    ObjectPainting object_painting;
    for (const PrintObject *print_object : objects) {
        const Transform3d transformation{print_object->trafo_centered()};
        const ModelVolumePtrs &volumes{print_object->model_object()->volumes};
        object_painting.emplace(print_object, ModelInfo::Painting{transformation, volumes});
    }

    ObjectShells seam_data{partition_to_shells(objects, params, object_painting, throw_if_canceled)};
    this->params = params;

    if (this->params.seam_preference != spNearest) {
        this->seams_per_object =
            precalculate_seams(params, std::move(seam_data), throw_if_canceled);
    } else {
        this->perimeters_per_layer = sort_to_layers(std::move(seam_data));
    }

    BOOST_LOG_TRIVIAL(debug) << "SeamPlacer: init: end";
}

const SeamPerimeterChoice &choose_closest_seam(
    const std::vector<SeamPerimeterChoice> &seams, const Polygon &loop_polygon
) {
    BoundingBoxes choose_from;
    choose_from.reserve(seams.size());
    for (const SeamPerimeterChoice &choice : seams) {
        choose_from.push_back(choice.bounding_box);
    }

    const std::size_t choice_index{
        Geometry::pick_closest_bounding_box(loop_polygon.bounding_box(), choose_from).first};

    return seams[choice_index];
}

std::pair<std::size_t, Vec2d> project_to_extrusion_loop(
    const SeamChoice &seam_choice, const Perimeters::Perimeter &perimeter, const Linesf &loop_lines
) {
    const AABBTreeLines::LinesDistancer<Linef> distancer{loop_lines};

    const bool is_at_vertex{seam_choice.previous_index == seam_choice.next_index};
    const Vec2d edge{
        perimeter.positions[seam_choice.next_index] -
        perimeter.positions[seam_choice.previous_index]};
    const Vec2d normal{
        is_at_vertex ?
            Geometry::get_polygon_normal(perimeter.positions, seam_choice.previous_index, 0.1) :
            Geometry::get_normal(edge)};

    double depth{distancer.distance_from_lines<false>(seam_choice.position)};
    const Vec2d final_position{seam_choice.position - normal * depth};

    auto [_, loop_line_index, loop_point] = distancer.distance_from_lines_extra<false>(final_position
    );
    return {loop_line_index, loop_point};
}

std::optional<Vec2d> offset_along_loop_lines(
    const Vec2d &point,
    const std::size_t loop_line_index,
    const Linesf &loop_lines,
    const double offset
) {
    double distance{0};
    Vec2d previous_point{point};
    std::optional<Vec2d> offset_point;
    Geometry::visit_near_forward(loop_line_index, loop_lines.size(), [&](std::size_t index) {
        const Vec2d next_point{loop_lines[index].b};
        const Vec2d edge{next_point - previous_point};

        if (distance + edge.norm() > offset) {
            const double remaining_distance{offset - distance};
            offset_point = previous_point + remaining_distance * edge.normalized();
            return true;
        }

        distance += edge.norm();
        previous_point = next_point;

        return false;
    });

    return offset_point;
}

double get_angle(const SeamChoice &seam_choice, const Perimeters::Perimeter &perimeter) {
    const bool is_at_vertex{seam_choice.previous_index == seam_choice.next_index};
    return is_at_vertex ? perimeter.angles[seam_choice.previous_index] : 0.0;
}

Point finalize_seam_position(
    const Polygon &loop_polygon,
    const SeamChoice &seam_choice,
    const Perimeters::Perimeter &perimeter,
    const double loop_width,
    const bool do_staggering
) {
    const Linesf loop_lines{to_unscaled_linesf({ExPolygon{loop_polygon}})};
    const auto [loop_line_index, loop_point]{
        project_to_extrusion_loop(seam_choice, perimeter, loop_lines)};

    // ExtrusionRole::Perimeter is inner perimeter.
    if (do_staggering) {
        const double depth = (loop_point - seam_choice.position).norm() -
            loop_width / 2.0;
        const double angle{get_angle(seam_choice, perimeter)};
        const double initial_offset{angle > 0 ? angle / 2.0 * depth : 0.0};
        const double additional_offset{angle < 0 ? std::cos(angle / 2.0) * depth : depth};

        const double staggering_offset{initial_offset + additional_offset};

        std::optional<Vec2d> staggered_point{
            offset_along_loop_lines(loop_point, loop_line_index, loop_lines, staggering_offset)};

        if (staggered_point) {
            return scaled(*staggered_point);
        }
    }

    return scaled(loop_point);
}

std::pair<SeamChoice, std::size_t> place_seam_near(
    const std::vector<BoundedPerimeter> &layer_perimeters,
    const ExtrusionLoop &loop,
    const Point &position,
    const double max_detour
) {
    BoundingBoxes choose_from;
    choose_from.reserve(layer_perimeters.size());
    for (const BoundedPerimeter &perimeter : layer_perimeters) {
        choose_from.push_back(perimeter.bounding_box);
    }

    const Polygon loop_polygon{Geometry::to_polygon(loop)};

    const std::size_t choice_index{
        Geometry::pick_closest_bounding_box(loop_polygon.bounding_box(), choose_from).first};

    Seams::Aligned::Impl::Nearest nearest{unscaled(position), max_detour};

    const SeamChoice choice{Seams::choose_seam_point(layer_perimeters[choice_index].perimeter, nearest)};

    return {choice, choice_index};
}

int get_perimeter_count(const Layer *layer){
    int count{0};
    for (const LayerRegion *layer_region : layer->regions()) {
        for (const ExtrusionEntity *ex_entity : layer_region->perimeters()) {
            if (ex_entity->is_collection()) { //collection of inner, outer, and overhang perimeters
                count += static_cast<const ExtrusionEntityCollection*>(ex_entity)->entities.size();
            }
            else {
                count += 1;
            }
        }
    }
    return count;
}

Point Placer::place_seam(const Layer *layer, const ExtrusionLoop &loop, const Point &last_pos) const {
    const PrintObject *po = layer->object();
    // Must not be called with supprot layer.
    assert(dynamic_cast<const SupportLayer *>(layer) == nullptr);
    // Object layer IDs are incremented by the number of raft layers.
    assert(layer->id() >= po->slicing_parameters().raft_layers());
    const size_t layer_index = layer->id() - po->slicing_parameters().raft_layers();

    const Polygon loop_polygon{Geometry::to_polygon(loop)};

    const bool do_staggering{this->params.staggered_inner_seams && loop.role() == ExtrusionRole::Perimeter};
    const double loop_width{loop.paths.empty() ? 0.0 : loop.paths.front().width()};


    if (this->params.seam_preference == spNearest) {
        const std::vector<BoundedPerimeter> &perimeters{this->perimeters_per_layer.at(po)[layer_index]};
        const auto [seam_choice, perimeter_index] = place_seam_near(perimeters, loop, last_pos, this->params.max_nearest_detour);
        return finalize_seam_position(loop_polygon, seam_choice, perimeters[perimeter_index].perimeter, loop_width, do_staggering);
    } else {
        const std::vector<SeamPerimeterChoice> &seams_on_perimeters{this->seams_per_object.at(po)[layer_index]};

        // Special case.
        // If there are only two perimeters and the current perimeter is hole (clockwise).
        const int perimeter_count{get_perimeter_count(layer)};
        const bool has_2_or_3_perimeters{perimeter_count == 2 || perimeter_count == 3};
        if (has_2_or_3_perimeters) {
            if (seams_on_perimeters.size() == 2 &&
                seams_on_perimeters[0].perimeter.is_hole !=
                    seams_on_perimeters[1].perimeter.is_hole) {
                const SeamPerimeterChoice &seam_perimeter_choice{
                    seams_on_perimeters[0].perimeter.is_hole ? seams_on_perimeters[1] :
                                                               seams_on_perimeters[0]};
                return finalize_seam_position(
                    loop_polygon, seam_perimeter_choice.choice, seam_perimeter_choice.perimeter,
                    loop_width, do_staggering
                );
            }
        }

        const SeamPerimeterChoice &seam_perimeter_choice{choose_closest_seam(seams_on_perimeters, loop_polygon)};
        return finalize_seam_position(loop_polygon, seam_perimeter_choice.choice, seam_perimeter_choice.perimeter, loop_width, do_staggering);
    }
}
} // namespace Slic3r::Seams
