// Quasizero Slicer — QZmini refill event planner and G-code transformer.
// Part of Quasizero Slicer, a fork of OrcaSlicer. GNU AGPLv3.
//
// Streams slicer-generated G-code (per-layer chunks), tracks consumed
// biomaterial through QzGcodeStateMachine and inserts adapter-based refill
// sequences at safe, non-extruding boundaries (travel moves or layer
// transitions). Never splits extrusion segments or G2/G3 arcs. If no safe
// boundary exists before the configured usable capacity is exceeded, the
// export must fail: 'failed()' becomes true with a clear error message.
#pragma once

#include "QzVolumetricModel.hpp"
#include "QzGcodeStateMachine.hpp"
#include "QzFirmwareAdapter.hpp"

#include <string>
#include <vector>

namespace Slic3r { namespace QuasiZero {

struct QzRefillOptions
{
    double refill_threshold_ml   = 120.0;
    double usable_capacity_ml    = 120.0; // hard limit for safe-boundary search
    double park_x = 10.0, park_y = 10.0, park_z_lift = 20.0;
    bool   plunger_reset = true;
    double plunger_reset_feedrate = 300.0; // mm/min (E axis)
    bool   prime_after_refill = false;
    double prime_ml = 1.0;
    double prime_feedrate = 120.0;         // mm/min (E axis)
    bool   emit_preview_tag = true;        // adds the reserved pause tag for Preview markers
    double travel_feedrate_mm_min = 3000.0;
    double z_feedrate_mm_min = 600.0;
    // The machine start G-code is emitted outside the layer stream; the initial
    // E mode must therefore be provided (OrcaSlicer: use_relative_e_distances).
    bool   initial_e_relative = false;
    QzFirmwareFamily family = QzFirmwareFamily::Marlin;
    std::string pause_gcode;               // resolved via qz_pause_command()
};

struct QzRefillEvent
{
    int    index = 0;
    double volume_ml = 0;  // consumed during the finished cycle
    double z = 0;
};

class QzRefillProcessor
{
public:
    QzRefillProcessor(const QzVolumetricParams &params, const QzRefillOptions &options)
        : m_params(params), m_options(options)
    {
        m_sm.force_modes(true, !options.initial_e_relative, 0.0);
    }

    // Transform one chunk (typically a whole layer). Newlines preserved.
    std::string process(const std::string &chunk);

    const std::vector<QzRefillEvent> &events() const { return m_events; }
    bool failed() const { return m_failed; }
    const std::string &error() const { return m_error; }

    // Total consumed (deposited) volume seen so far, ml.
    double consumed_ml() const { return m_params.ml_from_e(m_sm.state().deposited_e_total); }

private:
    QzVolumetricParams  m_params;
    QzRefillOptions     m_options;
    QzGcodeStateMachine m_sm;
    std::vector<QzRefillEvent> m_events;
    bool        m_armed  = false;
    bool        m_failed = false;
    std::string m_error;
    std::string m_last_feature_line;   // last ";TYPE:"/"; FEATURE:" line seen
    bool        m_config_checked = false;

    std::string make_sequence();
    void        check_capacity_exceeded();
};

}} // namespace Slic3r::QuasiZero
