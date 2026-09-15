// Quasizero Slicer — G-code state machine (implementation). GNU AGPLv3.
#include "QzGcodeStateMachine.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace Slic3r { namespace QuasiZero {

namespace {

inline const char *skip_ws(const char *p) { while (*p == ' ' || *p == '\t') ++p; return p; }

// True if the comment marks a layer transition (Orca reserved tag or the
// common machine layer_change hooks).
inline bool is_layer_change_comment(const std::string &c)
{
    return c.find("CHANGE_LAYER")        != std::string::npos ||
           c.find("AFTER_LAYER_CHANGE")  != std::string::npos ||
           c.find("LAYER_CHANGE")        != std::string::npos;
}

} // namespace

void QzGcodeStateMachine::apply_e_delta(double de, QzLineInfo &info)
{
    info.physical_e_delta = de;
    m_state.physical_e_since_mark += de;
    if (de < 0.0) {
        // retraction: physically pulls the plunger back
        m_state.pending_retract_e += -de;
    } else if (de > 0.0) {
        // unretract first, only the remainder is newly deposited material
        double unretract = de < m_state.pending_retract_e ? de : m_state.pending_retract_e;
        m_state.pending_retract_e -= unretract;
        double deposited = de - unretract;
        info.deposited_e_delta = deposited;
        m_state.deposited_e_total      += deposited;
        m_state.deposited_e_since_mark += deposited;
        info.is_extruding = de > 0.0;
    }
}

QzLineInfo QzGcodeStateMachine::process_line(const std::string &line)
{
    QzLineInfo info;
    const char *p = skip_ws(line.c_str());
    if (*p == '\0') { info.kind = QzLineInfo::Kind::Empty; return info; }

    if (*p == ';') {
        info.kind = QzLineInfo::Kind::Comment;
        std::string comment(p + 1);
        if (is_layer_change_comment(comment)) {
            info.is_layer_change = true;
            ++m_state.layer_count;
        }
        return info;
    }

    // command word
    char letter = (char)std::toupper((unsigned char)*p);
    if (letter != 'G' && letter != 'M' && letter != 'T') {
        info.kind = QzLineInfo::Kind::Other;
        return info;
    }
    char *endp = nullptr;
    long number = std::strtol(p + 1, &endp, 10);
    if (endp == p + 1) { info.kind = QzLineInfo::Kind::Other; return info; }

    // parameters (stop at comment)
    const char *q = endp;
    while (*q != '\0' && *q != ';') {
        q = skip_ws(q);
        char axis = (char)std::toupper((unsigned char)*q);
        if (axis == '\0' || axis == ';') break;
        char *vend = nullptr;
        double v = std::strtod(q + 1, &vend);
        if (vend == q + 1) { ++q; continue; }
        switch (axis) {
        case 'X': info.has_x = true; info.x = v; break;
        case 'Y': info.has_y = true; info.y = v; break;
        case 'Z': info.has_z = true; info.z = v; break;
        case 'E': info.has_e = true; info.e = v; break;
        case 'F': info.has_f = true; info.f = v; break;
        default: break;
        }
        q = vend;
    }

    auto update_axis = [this](double &axis_pos, bool has, double v) {
        if (has) axis_pos = m_state.xyz_absolute ? v : axis_pos + v;
    };

    if (letter == 'G') {
        switch (number) {
        case 0: case 1: case 2: case 3:
        {
            info.kind = (number >= 2) ? QzLineInfo::Kind::ArcMove : QzLineInfo::Kind::Move;
            if (info.has_f) m_state.feedrate = info.f;
            update_axis(m_state.x, info.has_x, info.x);
            update_axis(m_state.y, info.has_y, info.y);
            update_axis(m_state.z, info.has_z, info.z);
            if (info.has_e) {
                double de = m_state.e_absolute ? info.e - m_state.logical_e : info.e;
                m_state.logical_e = m_state.e_absolute ? info.e : m_state.logical_e + info.e;
                apply_e_delta(de, info);
            }
            info.is_travel = !info.has_e || info.physical_e_delta == 0.0;
            break;
        }
        case 28:
            info.kind = QzLineInfo::Kind::Home;
            // no axis args homes all
            if (!info.has_x && !info.has_y && !info.has_z) { m_state.x = m_state.y = m_state.z = 0; }
            else {
                if (info.has_x) m_state.x = 0;
                if (info.has_y) m_state.y = 0;
                if (info.has_z) m_state.z = 0;
            }
            break;
        case 90: info.kind = QzLineInfo::Kind::SetXyzAbsolute; m_state.xyz_absolute = true;  m_state.e_absolute = true;  break;
        case 91: info.kind = QzLineInfo::Kind::SetXyzRelative; m_state.xyz_absolute = false; m_state.e_absolute = false; break;
        case 92:
            info.kind = QzLineInfo::Kind::SetPosition;
            // logical reset only; the plunger does not move
            if (info.has_e) m_state.logical_e = info.e;
            if (info.has_x) m_state.x = info.x;
            if (info.has_y) m_state.y = info.y;
            if (info.has_z) m_state.z = info.z;
            break;
        default: info.kind = QzLineInfo::Kind::Other; break;
        }
        return info;
    }

    // M codes
    switch (number) {
    case 82: info.kind = QzLineInfo::Kind::SetEAbsolute; m_state.e_absolute = true;  break;
    case 83: info.kind = QzLineInfo::Kind::SetERelative; m_state.e_absolute = false; break;
    case 0: case 1: case 25: case 226: case 600: case 601:
        info.kind = QzLineInfo::Kind::Pause; break;
    default: info.kind = QzLineInfo::Kind::Other; break;
    }
    return info;
}

void QzGcodeStateMachine::mark_refill(double restored_logical_e, double primed_e)
{
    m_state.logical_e = restored_logical_e;
    m_state.pending_retract_e = 0.0;
    // The new cycle starts with the prime volume already consumed.
    m_state.physical_e_since_mark  = primed_e;
    m_state.deposited_e_since_mark = primed_e;
}

void QzGcodeStateMachine::force_modes(bool xyz_absolute, bool e_absolute, double feedrate)
{
    m_state.xyz_absolute = xyz_absolute;
    m_state.e_absolute   = e_absolute;
    if (feedrate > 0) m_state.feedrate = feedrate;
}

}} // namespace Slic3r::QuasiZero
