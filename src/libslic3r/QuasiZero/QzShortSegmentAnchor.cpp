// Quasizero Slicer - Short-Segment Anchoring implementation. GNU AGPLv3.
#include "QzShortSegmentAnchor.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Slic3r { namespace QuasiZero {

namespace {

// parse a G-code word (e.g. 'X') from the command part of a line
bool parse_word(const std::string &cmd, char word, double &value)
{
    for (size_t i = 0; i < cmd.size(); ++i) {
        const char c = cmd[i];
        if (c == ';') break;
        if ((c == word || c == (char)(word + 32)) && (i == 0 || cmd[i - 1] == ' ' || cmd[i - 1] == '\t')) {
            char *end = nullptr;
            const double v = std::strtod(cmd.c_str() + i + 1, &end);
            if (end != cmd.c_str() + i + 1) { value = v; return true; }
        }
    }
    return false;
}

bool starts_with_cmd(const std::string &line, const char *cmd)
{
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    const size_t n = std::strlen(cmd);
    if (line.compare(i, n, cmd) != 0) return false;
    const char next = i + n < line.size() ? line[i + n] : '\0';
    return next == ' ' || next == '\t' || next == ';' || next == '\0' || next == '\r';
}

} // namespace

std::string QzShortSegmentAnchor::process(const std::string &chunk)
{
    std::string out;
    out.reserve(chunk.size() + 256);
    std::string data = m_carry + chunk;
    m_carry.clear();
    size_t start = 0;
    while (start < data.size()) {
        const size_t nl = data.find('\n', start);
        if (nl == std::string::npos) {
            m_carry = data.substr(start);
            break;
        }
        handle_line(data.substr(start, nl - start + 1), out);
        start = nl + 1;
    }
    return out;
}

void QzShortSegmentAnchor::handle_line(const std::string &line, std::string &out)
{
    // modes
    if (starts_with_cmd(line, "G90")) { m_xyz_rel = false; }
    else if (starts_with_cmd(line, "G91")) { m_xyz_rel = true; }
    else if (starts_with_cmd(line, "M82")) { m_e_rel = false; }
    else if (starts_with_cmd(line, "M83")) { m_e_rel = true; }
    else if (starts_with_cmd(line, "G92")) {
        double e = 0.0;
        if (parse_word(line, 'E', e)) m_last_e = e;
    }

    const bool is_move = starts_with_cmd(line, "G1") || starts_with_cmd(line, "G0");
    double x = 0.0, y = 0.0, e = 0.0, f = 0.0;
    const bool has_x = is_move && parse_word(line, 'X', x);
    const bool has_y = is_move && parse_word(line, 'Y', y);
    const bool has_e = is_move && parse_word(line, 'E', e);
    const bool has_f = is_move && parse_word(line, 'F', f);

    double de = 0.0;
    if (has_e) de = m_e_rel ? e : e - m_last_e;
    const bool is_extrusion = is_move && (has_x || has_y) && has_e && de > 1e-9;
    const bool is_xy_travel = is_move && (has_x || has_y) && (!has_e || de <= 1e-9);

    double seg_len = 0.0;
    if (is_move && (has_x || has_y) && m_has_xy) {
        const double nx = m_xyz_rel ? m_last_x + (has_x ? x : 0.0) : (has_x ? x : m_last_x);
        const double ny = m_xyz_rel ? m_last_y + (has_y ? y : 0.0) : (has_y ? y : m_last_y);
        seg_len = std::sqrt((nx - m_last_x) * (nx - m_last_x) + (ny - m_last_y) * (ny - m_last_y));
    }

    if (is_extrusion) {
        if (!m_buffering && !m_in_long) {
            // island starts
            m_buffering  = true;
            m_island_len = 0.0;
            m_island_e0  = m_last_e;
            m_island_f0  = m_last_f;
            m_buf.clear();
        }
        if (m_buffering) {
            m_island_len += seg_len;
            m_buf.push_back(line);
            if (m_island_len > m_o.max_length_mm || m_buf.size() > 500)
                flush_island(out, false); // long island: pass through untouched
        }
        else
            out += line;
    }
    else {
        if (m_buffering)
            flush_island(out, true); // short island ended -> anchor it
        if (m_in_long && !is_extrusion && (is_xy_travel || !is_move))
            m_in_long = false;

        if (m_slow_depart && is_xy_travel) {
            // clamp the first travel leaving the island, then restore modal F
            const double cap = m_o.depart_speed_mms * 60.0;
            const double restore = has_f ? f : m_last_f;
            std::string mod = line;
            while (!mod.empty() && (mod.back() == '\n' || mod.back() == '\r')) mod.pop_back();
            if (has_f && f > cap) {
                char fb[32]; std::snprintf(fb, sizeof(fb), "F%.0f", cap);
                const size_t fp = mod.find_first_of("Ff");
                size_t fpos = std::string::npos;
                for (size_t i2 = 0; i2 < mod.size(); ++i2)
                    if ((mod[i2] == 'F' || mod[i2] == 'f') && (i2 == 0 || mod[i2-1] == ' ' || mod[i2-1] == '\t')) { fpos = i2; break; }
                (void)fp;
                if (fpos != std::string::npos) {
                    size_t fend = fpos + 1;
                    while (fend < mod.size() && mod[fend] != ' ' && mod[fend] != '\t' && mod[fend] != ';') ++fend;
                    mod = mod.substr(0, fpos) + fb + mod.substr(fend);
                }
                out += mod + " ; QZ_SSA_DEPART\n";
            }
            else if (!has_f && m_last_f > cap) {
                char fb[48]; std::snprintf(fb, sizeof(fb), " F%.0f ; QZ_SSA_DEPART", cap);
                out += mod + fb + "\n";
            }
            else
                out += line;
            if (restore > cap) {
                char rb[64]; std::snprintf(rb, sizeof(rb), "G1 F%.0f ; QZ_SSA_RESTORE_F\n", restore);
                out += rb;
            }
            m_slow_depart = false;
        }
        else
            out += line;
    }

    // state updates (logical, after routing the line)
    if (is_move) {
        if (has_x) m_last_x = m_xyz_rel ? m_last_x + x : x;
        if (has_y) m_last_y = m_xyz_rel ? m_last_y + y : y;
        if (has_x || has_y) m_has_xy = true;
        if (has_e) m_last_e = m_e_rel ? m_last_e + e : e;
        if (has_f) m_last_f = f;
    }
}

void QzShortSegmentAnchor::flush_island(std::string &out, bool anchored)
{
    if (anchored && !m_buf.empty()) {
        char buf[128];
        if (m_o.extra_prime_e > 1e-9) {
            if (m_e_rel) {
                std::snprintf(buf, sizeof(buf), "G1 E%.5f F%.0f ; QZ_SSA_PRIME\n", m_o.extra_prime_e, m_o.prime_feedrate);
                out += buf;
            }
            else {
                std::snprintf(buf, sizeof(buf), "G1 E%.5f F%.0f ; QZ_SSA_PRIME\n", m_island_e0 + m_o.extra_prime_e, m_o.prime_feedrate);
                out += buf;
                std::snprintf(buf, sizeof(buf), "G92 E%.5f ; QZ_SSA_PRIME_RESET\n", m_island_e0);
                out += buf;
            }
            if (m_island_f0 > 0.0) {
                std::snprintf(buf, sizeof(buf), "G1 F%.0f ; QZ_SSA_RESTORE_F\n", m_island_f0);
                out += buf;
            }
        }
        for (const std::string &l : m_buf) out += l;
        std::snprintf(buf, sizeof(buf), "G4 P%d ; QZ_SSA_DWELL\n", m_o.dwell_ms);
        out += buf;
        m_slow_depart = true;
        ++m_count;
    }
    else {
        for (const std::string &l : m_buf) out += l;
        m_in_long = anchored ? false : true;
    }
    m_buf.clear();
    m_buffering = false;
}

}} // namespace Slic3r::QuasiZero
