// Quasizero Slicer — state-aware G-code tracking for QZmini refill planning.
// Part of Quasizero Slicer, a fork of OrcaSlicer. GNU AGPLv3.
//
// Tracks coordinate modes (G90/G91), E-axis modes (M82/M83), G92 resets,
// XYZ/E/F state, deposition vs retraction (retract/unretract pairs are never
// counted as deposited material), layer transitions, travel moves, arcs,
// existing pause commands and comments. Pure C++17, no libslic3r dependencies,
// so behaviour is verifiable by standalone unit tests.
#pragma once

#include <string>
#include <cstdint>

namespace Slic3r { namespace QuasiZero {

struct QzLineInfo
{
    enum class Kind : uint8_t {
        Empty, Comment, Move, ArcMove, Home, SetXyzAbsolute, SetXyzRelative,
        SetEAbsolute, SetERelative, SetPosition, Pause, Other
    };
    Kind   kind = Kind::Empty;
    bool   has_x = false, has_y = false, has_z = false, has_e = false, has_f = false;
    double x = 0, y = 0, z = 0, e = 0, f = 0;
    bool   is_travel = false;        // motion without extrusion
    bool   is_extruding = false;     // positive physical E delta
    bool   is_layer_change = false;  // layer-transition comment tag
    double physical_e_delta = 0;     // signed plunger displacement of this line (E units)
    double deposited_e_delta = 0;    // retract-safe newly deposited E units
};

struct QzGcodeState
{
    bool   xyz_absolute = true;
    bool   e_absolute   = true;   // Marlin default; slicer start G-code usually sets M83
    double x = 0, y = 0, z = 0;
    double logical_e = 0;         // current logical E coordinate
    double feedrate  = 0;
    double pending_retract_e = 0; // physically retracted, awaiting unretract
    double deposited_e_total = 0; // net deposited E units (retract/unretract safe)
    double physical_e_since_mark = 0; // net plunger advance since last mark (for plunger reset)
    double deposited_e_since_mark = 0;
    int    layer_count = 0;
};

class QzGcodeStateMachine
{
public:
    // Parse one line (without trailing newline) and update state.
    QzLineInfo process_line(const std::string &line);

    const QzGcodeState &state() const { return m_state; }

    // Called after a refill: plunger physically reset; a fresh syringe is in
    // place. 'primed_e' is the physical prime executed in the park area, which
    // belongs to the new cycle's budget.
    void mark_refill(double restored_logical_e, double primed_e);

    // Overwrite tracked modes/feedrate (used after emitting a generated
    // sequence whose final state is known by construction).
    void force_modes(bool xyz_absolute, bool e_absolute, double feedrate);

private:
    QzGcodeState m_state;
    void apply_e_delta(double de, QzLineInfo &info);
};

}} // namespace Slic3r::QuasiZero
