// Quasizero Slicer - Short-Segment Anchoring for paste extrusion. GNU AGPLv3.
//
// Paste islands shorter than a threshold tend to leave the bed glued to the
// nozzle: they are printed after a travel+retraction (so they under-prime) and
// the nozzle departs at full speed before the paste releases. This filter
// buffers each extrusion island; when a short one ends it emits:
//   1. an extra prime before the island (logical E preserved via G92 in
//      absolute mode - same physical/logical split the Refill Assist uses),
//   2. a G4 dwell after the island so the paste lets go of the nozzle,
//   3. a speed clamp on the first travel move leaving the island
//      (modal feedrate restored right after).
#ifndef slic3r_QzShortSegmentAnchor_hpp_
#define slic3r_QzShortSegmentAnchor_hpp_

#include <string>
#include <vector>

namespace Slic3r { namespace QuasiZero {

struct QzSsaOptions
{
    double max_length_mm      = 2.0;   // islands up to this XY length get anchored
    int    dwell_ms           = 300;   // pause after the island
    double extra_prime_e      = 0.3;   // E units primed before the island
    double prime_feedrate     = 300.0; // mm/min for the prime move
    double depart_speed_mms   = 10.0;  // first travel out of the island capped to this
    bool   initial_e_relative = false;
};

class QzShortSegmentAnchor
{
public:
    explicit QzShortSegmentAnchor(const QzSsaOptions &o) : m_o(o), m_e_rel(o.initial_e_relative) {}

    // Transform one chunk (typically a whole layer). Newlines preserved.
    std::string process(const std::string &chunk);

    int anchored_islands() const { return m_count; }

private:
    void handle_line(const std::string &line, std::string &out);
    void flush_island(std::string &out, bool anchored);

    QzSsaOptions m_o;
    bool   m_e_rel        = false;
    bool   m_xyz_rel      = false;
    double m_last_e       = 0.0;   // logical E (absolute mode)
    double m_last_x       = 0.0, m_last_y = 0.0;
    bool   m_has_xy       = false;
    double m_last_f       = 0.0;   // modal feedrate, mm/min
    bool   m_buffering    = false;
    bool   m_in_long      = false; // inside an island already flushed as long
    double m_island_len   = 0.0;
    double m_island_e0    = 0.0;   // logical E at island start (absolute mode prime)
    double m_island_f0    = 0.0;   // modal F before the island (restored after prime)
    bool   m_slow_depart  = false;
    std::vector<std::string> m_buf;
    std::string m_carry;           // partial line across chunks
    int    m_count        = 0;
};

}} // namespace Slic3r::QuasiZero

#endif
