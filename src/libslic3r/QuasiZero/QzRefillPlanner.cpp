// Quasizero Slicer — refill planner (implementation). GNU AGPLv3.
#include "QzRefillPlanner.hpp"

#include <cstdio>
#include <sstream>

namespace Slic3r { namespace QuasiZero {

namespace {
inline std::string fmt(const char *format, double v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), format, v);
    return std::string(buf);
}
} // namespace

void QzRefillProcessor::check_capacity_exceeded()
{
    if (m_failed || !m_armed)
        return;
    const double cycle_ml = m_params.ml_from_e(m_sm.state().deposited_e_since_mark);
    if (cycle_ml > m_options.usable_capacity_ml) {
        m_failed = true;
        std::ostringstream os;
        os << "QZmini Refill Assist: no safe pause boundary was found before the configured usable "
              "syringe capacity was exceeded (consumed "
           << fmt("%.1f", cycle_ml) << " ml in the current cycle, usable capacity "
           << fmt("%.1f", m_options.usable_capacity_ml) << " ml, refill threshold "
           << fmt("%.1f", m_options.refill_threshold_ml) << " ml). "
              "The G-code contains a continuous extrusion region longer than the safety reserve. "
              "Export was stopped instead of generating unsafe output. Reduce the refill threshold, "
              "verify the usable capacity, or simplify the continuous toolpath region.";
        m_error = os.str();
    }
}

std::string QzRefillProcessor::make_sequence()
{
    const QzGcodeState st = m_sm.state(); // copy: pre-sequence state
    const double cycle_ml   = m_params.ml_from_e(st.deposited_e_since_mark);
    const double reset_e    = st.physical_e_since_mark; // physical plunger advance this cycle
    const double prime_e    = m_options.prime_after_refill ? m_params.e_from_ml(m_options.prime_ml) : 0.0;
    const int    index      = (int)m_events.size() + 1;

    std::ostringstream g;
    g << "; QZ_REFILL_BEGIN\n";
    g << "; QZ_REFILL_INDEX=" << index << "\n";
    g << "; QZ_REFILL_VOLUME_ML=" << fmt("%.2f", cycle_ml) << "\n";
    g << qz_family_comment(m_options.family) << "\n";
    if (m_options.emit_preview_tag)
        g << "; PAUSE_PRINTING\n"; // reserved tag: creates the pause marker in Preview
    g << "M400 ; wait for queued moves to complete\n";
    // save-state comment block (traceability in the file)
    g << "; QZ_SAVED_X=" << fmt("%.3f", st.x) << " Y=" << fmt("%.3f", st.y)
      << " Z=" << fmt("%.3f", st.z) << " E=" << fmt("%.5f", st.logical_e)
      << " F=" << fmt("%.0f", st.feedrate)
      << " XYZ_MODE=" << (st.xyz_absolute ? "ABS" : "REL")
      << " E_MODE=" << (st.e_absolute ? "ABS(M82)" : "REL(M83)") << "\n";
    // 1) relative Z lift (safe regardless of current mode)
    g << "G91 ; relative for Z lift\n";
    g << "G1 Z" << fmt("%.3f", m_options.park_z_lift) << " F" << fmt("%.0f", m_options.z_feedrate_mm_min) << " ; lift\n";
    g << "G90 ; absolute XY for park\n";
    // 2) park
    g << "G0 X" << fmt("%.3f", m_options.park_x) << " Y" << fmt("%.3f", m_options.park_y)
      << " F" << fmt("%.0f", m_options.travel_feedrate_mm_min) << " ; park for refill\n";
    // 3) plunger reset in relative E mode; never assume absolute E-<total> is correct
    g << "M83 ; relative E for plunger reset\n";
    if (m_options.plunger_reset && reset_e > 0.0)
        g << "G1 E-" << fmt("%.5f", reset_e) << " F" << fmt("%.0f", m_options.plunger_reset_feedrate)
          << " ; retract plunger by material consumed this cycle\n";
    // 4) firmware pause
    g << m_options.pause_gcode << "\n";
    g << "; QZ: user refills or replaces the syringe, then resumes.\n";
    g << "; QZ: the plunger is NOT advanced back to its previous depth; new material replaced the old.\n";
    // 5) optional prime in the park area (relative E still active)
    if (m_options.prime_after_refill && prime_e > 0.0)
        g << "G1 E" << fmt("%.5f", prime_e) << " F" << fmt("%.0f", m_options.prime_feedrate)
          << " ; prime after refill\n";
    // 6) restore logical E coordinate and E mode
    if (st.e_absolute) {
        g << "G92 E" << fmt("%.5f", st.logical_e) << " ; restore logical E coordinate\n";
        g << "M82 ; restore absolute E mode\n";
    } else {
        g << "G92 E" << fmt("%.5f", st.logical_e) << " ; normalize logical E (relative mode)\n";
    }
    // 7) return XY first (Z stays lifted), then lower Z
    g << "G0 X" << fmt("%.3f", st.x) << " Y" << fmt("%.3f", st.y)
      << " F" << fmt("%.0f", m_options.travel_feedrate_mm_min) << " ; return above print\n";
    g << "G1 Z" << fmt("%.3f", st.z) << " F" << fmt("%.0f", m_options.z_feedrate_mm_min) << " ; lower to printing height\n";
    // 8) restore modes and feedrate
    if (!st.xyz_absolute)
        g << "G91 ; restore relative XYZ mode\n";
    if (st.feedrate > 0.0)
        g << "G1 F" << fmt("%.0f", st.feedrate) << " ; restore feedrate\n";
    g << "; QZ_REFILL_END\n";

    m_events.push_back({ index, cycle_ml, st.z });
    // Update tracked state to the sequence's known end state.
    m_sm.mark_refill(st.logical_e, prime_e);
    m_sm.force_modes(st.xyz_absolute, st.e_absolute, st.feedrate);
    return g.str();
}

std::string QzRefillProcessor::process(const std::string &chunk)
{
    if (m_failed)
        return chunk;
    std::string out;
    out.reserve(chunk.size() + 256);
    size_t pos = 0;
    while (pos < chunk.size()) {
        size_t eol = chunk.find('\n', pos);
        const bool has_nl = eol != std::string::npos;
        std::string line = chunk.substr(pos, has_nl ? eol - pos : std::string::npos);
        pos = has_nl ? eol + 1 : chunk.size();

        // Never re-process our own sequences (they are emitted pre-tracked).
        QzLineInfo info = m_sm.process_line(line);

        // Arm when the finished cycle volume reaches the threshold.
        if (!m_armed) {
            const double cycle_ml = m_params.ml_from_e(m_sm.state().deposited_e_since_mark);
            if (cycle_ml >= m_options.refill_threshold_ml)
                m_armed = true;
        }

        const bool safe_boundary =
            (info.kind == QzLineInfo::Kind::Move && info.is_travel) ||
            info.is_layer_change;

        out += line;
        if (has_nl) out += '\n';

        if (m_armed && safe_boundary) {
            // Insert AFTER this completed non-extruding line: the tracked state
            // (position, modes, feedrate) matches the machine state exactly at
            // this point, and no extrusion segment or arc is split.
            out += make_sequence();
            m_armed = false;
        }

        check_capacity_exceeded();
        if (m_failed)
            break;
    }
    return out;
}

}} // namespace Slic3r::QuasiZero
