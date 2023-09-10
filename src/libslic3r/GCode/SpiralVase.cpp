///|/ Copyright (c) Prusa Research 2017 - 2021 Vojtěch Bubník @bubnikv, Lukáš Matěna @lukasmatena
///|/
///|/ ported from lib/Slic3r/GCode/SpiralVase.pm:
///|/ Copyright (c) Prusa Research 2017 Vojtěch Bubník @bubnikv
///|/ Copyright (c) Slic3r 2013 - 2014 Alessandro Ranellucci @alranel
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "SpiralVase.hpp"
#include "GCode.hpp"
#include <sstream>

namespace Slic3r {

std::string SpiralVase::process_layer(const std::string &gcode)
{
    /*  This post-processor relies on several assumptions:
        - all layers are processed through it, including those that are not supposed
          to be transformed, in order to update the reader with the XY positions
        - each call to this method includes a full layer, with a single Z move
          at the beginning
        - each layer is composed by suitable geometry (i.e. a single complete loop)
        - loops were not clipped before calling this method  */

    // If we're not going to modify G-code, just feed it to the reader
    // in order to update positions.
    if (!m_enabled) {
        m_reader.parse_buffer(gcode);
        return gcode;
    }

    // Get total XY length for this layer by summing all extrusion moves.
    float total_layer_length = 0;
    float layer_height       = 0;
    float layer_z            = 0.f;

    {
        // FIXME Performance warning: This copies the GCodeConfig of the reader.
        GCodeReader r     = m_reader; // clone
        bool        set_z = false;
        r.parse_buffer(gcode,
                       [&total_layer_length, &layer_height, &layer_z, &set_z](GCodeReader &reader, const GCodeReader::GCodeLine &line) {
                           if (line.cmd_is("G1")) {
                               if (line.extruding(reader)) {
                                   total_layer_length += line.dist_XY(reader);
                               } else if (line.has(Z)) {
                                   layer_height += line.dist_Z(reader);
                                   if (!set_z) {
                                       layer_z = line.new_Z(reader);
                                       set_z   = true;
                                   }
                               }
                           }
                       });
    }

    // get a copy of the reader in case a last_layer_transition should be done
    bool        last_layer_transition = m_config.spiral_vase_flush_finish && m_last_layer && m_config.use_relative_e_distances.value;
    GCodeReader last_layer_reader;
    if (last_layer_transition) {
        last_layer_reader = m_reader; // clone
    }

    // Remove layer height from initial Z.
    float z = layer_z - layer_height;

    std::string new_gcode;
    // FIXME Tapering of the transition layer only works reliably with relative extruder distances.
    // For absolute extruder distances it will be switched off.
    // Tapering the absolute extruder distances requires to process every extrusion value after the first transition
    // layer.
    bool  transition          = m_transition_layer && m_config.use_relative_e_distances.value;
    float layer_height_factor = layer_height / total_layer_length;
    float len                 = 0.f;
    m_reader.parse_buffer(gcode, [&new_gcode, &z, total_layer_length, layer_height_factor, transition, &len](GCodeReader           &reader,
                                                                                                             GCodeReader::GCodeLine line) {
        if (line.cmd_is("G1")) {
            if (line.has_z()) {
                // If this is the initial Z move of the layer, replace it with a
                // (redundant) move to the last Z of previous layer.
                line.set(reader, Z, z);
                new_gcode += line.raw() + '\n';
                return;
            } else {
                float dist_XY = line.dist_XY(reader);
                if (dist_XY > 0) {
                    // horizontal move
                    if (line.extruding(reader)) {
                        len += dist_XY;
                        line.set(reader, Z, z + len * layer_height_factor);
                        if (transition && line.has(E))
                            // Transition layer, modulate the amount of extrusion from zero to the final value.
                            line.set(reader, E, line.value(E) * len / total_layer_length);
                        new_gcode += line.raw() + '\n';
                    }
                    return;

                    /*  Skip travel moves: the move to first perimeter point will
                        cause a visible seam when loops are not aligned in XY; by skipping
                        it we blend the first loop move in the XY plane (although the smoothness
                        of such blend depend on how long the first segment is; maybe we should
                        enforce some minimum length?).  */
                }
            }
        }
        new_gcode += line.raw() + '\n';
    });

    if (last_layer_transition) {
        // Repeat last layer on final height while reducing extrusion to zero to get a flush last layer.
        len = 0.f;

        last_layer_reader.parse_buffer(gcode, [&new_gcode, &layer_z, total_layer_length, layer_height_factor, transition,
                                               last_layer_transition, &len](GCodeReader &reader, GCodeReader::GCodeLine line) {
            if (line.cmd_is("G1")) {
                if (line.has_z()) {
                    // Set z to final layer_z.
                    // We should actually already be at this height.
                    line.set(reader, Z, layer_z);
                    new_gcode += line.raw() + '\n';
                    return;
                } else {
                    float dist_XY = line.dist_XY(reader);
                    if (dist_XY > 0) {
                        if (line.extruding(reader)) {
                            len += dist_XY;
                            if (line.has(E))
                                // Transition layer, modulate the amount of extrusion to zero.
                                line.set(reader, E, line.value(E) * (1.f - len / total_layer_length));
                            new_gcode += line.raw() + '\n';
                        }
                        return;
                    }
                }
            }

            // We need to remove the layer change comment before the last_layer_transition,
            // as otherwise the gcode preview will not work properly. The transition layer will be part of the last layer.
            // Note: we need to keep the other layer change commands though - e.g. resetting the relative E value.
            if (last_layer_transition && line.comment() == GCodeProcessor::reserved_tag(GCodeProcessor::ETags::Layer_Change)) {
                return;
            }
            new_gcode += line.raw() + '\n';
        });
    }

    return new_gcode;
}

} // namespace Slic3r
