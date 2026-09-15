// Quasizero Slicer — firmware-family adapters for QZmini refill sequences.
// Part of Quasizero Slicer, a fork of OrcaSlicer. GNU AGPLv3.
//
// Pause/resume behaviour differs per firmware family; there is deliberately
// no universal macro. Validation status per family is tracked in
// SUPPORTED_PRINTERS.md and repeated in generated G-code comments.
#pragma once

#include <string>

namespace Slic3r { namespace QuasiZero {

enum class QzFirmwareFamily {
    Marlin,          // initial validation target (Artillery Sidewinder X2)
    Klipper,         // requires printer.cfg support (min_extrude_temp, macros)
    RepRapFirmware,  // unverified template
    PrusaBuddy,      // unverified template
    BambuLab,        // EXPERIMENTAL / SLICE-ONLY
    Unknown
};

QzFirmwareFamily qz_family_from_flavor(const std::string &gcode_flavor);
const char *qz_family_name(QzFirmwareFamily f);
// Validation label required by the project rules (never claim more than tested).
const char *qz_family_validation_label(QzFirmwareFamily f);

// Resolve the pause command block.
//   strategy: "auto" | "M0" | "M25" | "M600" | "custom"
//   machine_pause_gcode: printer profile pause G-code (used by "auto" when set)
//   custom_gcode: used by "custom"
std::string qz_pause_command(QzFirmwareFamily family,
                             const std::string &strategy,
                             const std::string &machine_pause_gcode,
                             const std::string &custom_gcode);

// Family-specific cold-extrusion note emitted inside refill sequences.
std::string qz_family_comment(QzFirmwareFamily family);

}} // namespace Slic3r::QuasiZero
